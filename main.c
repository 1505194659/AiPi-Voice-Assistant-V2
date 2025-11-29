#include "FreeRTOS.h"
#include "task.h"

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "bl_fw_api.h"
#include "wifi_mgmr_ext.h"
#include "wifi_mgmr.h"

#include "bflb_irq.h"
#include "bflb_mtimer.h"
#include "bflb_gpio.h"
#include "bflb_i2s.h"
#include "bflb_dma.h"
#include "bflb_l1c.h"
#include "bl616_glb.h"
#include "rfparam_adapter.h"

#include "board.h"
#include "log.h"
#include "bsp_es8388.h"
#include "https_client.h"
#include "stt_client.h"
#include "deepseek_client.h"
#include "tts_client.h"
#include "config.h"
#include "vad.h"

#include <math.h>
#include <stdint.h>

#define DBG_TAG "MAIN"

// WiFi Configuration
#define WIFI_STACK_SIZE (1536)
#define TASK_PRIORITY_FW (16)

static TaskHandle_t wifi_fw_task;
static wifi_conf_t conf = {
    .country_code = "CN",
};

static volatile uint32_t sta_connect_status = 0;

// Audio Configuration
#define AUDIO_SAMPLE_RATE 16000  // 16kHz
#define AUDIO_MAX_RECORD_TIME 10 // Increased to 10 seconds
#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLE_RATE * 2 * 2 * 4)  // 256KB - 16-bit stereo, 4 seconds for overlap
#define AUDIO_CHUNK_SIZE (AUDIO_SAMPLE_RATE * 2 * 2 * 1)  // 1 second chunks for VAD processing (stereo)

static uint8_t *audio_buffer = NULL;
static volatile uint32_t audio_write_pos = 0;
static volatile uint32_t audio_recorded_size = 0;
static volatile bool recording_complete = false;
static volatile bool playback_complete = false;
static vad_state_t vad_state;

// Current ES8388 mode
static ES8388_Work_Mode current_es8388_mode = ES8388_RECORDING_MODE;

// Buffer to store the audio that triggered voice detection
// 2 second buffer (stereo 16-bit) = 128KB - dynamically allocated in PSRAM
#define TRIGGER_BUFFER_MS 2000
#define TRIGGER_BUFFER_SIZE (AUDIO_SAMPLE_RATE * 2 * 2 * TRIGGER_BUFFER_MS / 1000)
// VAD Threshold: Restored to 2000 as per user request to avoid false triggers
#define VOICE_ENERGY_THRESHOLD 2000
static uint8_t *trigger_audio_buffer = NULL;
static uint32_t trigger_audio_len = 0;
static bool has_trigger_audio = false;

// Device handles (non-static for external access by tts_client)
struct bflb_device_s *i2s0 = NULL;
struct bflb_device_s *dma0_ch0 = NULL;
struct bflb_device_s *dma0_ch1 = NULL;  // For TX (playback) - non-static for tts_client access

// DMA interrupt handler for RX (recording)
void dma0_ch0_isr(void *arg)
{
    // DMA completed the entire buffer
    LOG_I("DMA transfer complete (buffer full)\r\n");
    recording_complete = true;
    audio_recorded_size = AUDIO_BUFFER_SIZE;
    bflb_dma_channel_stop(dma0_ch0);
}

// DMA interrupt handler for TX (playback)
void dma0_ch1_isr(void *arg)
{
    LOG_I("Playback complete\r\n");
    playback_complete = true;
    bflb_dma_channel_stop(dma0_ch1);
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);
}

// Initialize I2S GPIO pins
void init_i2s_gpio(void)
{
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
    
    /* I2S_FS (Frame Select) on GPIO_PIN_13 */
    bflb_gpio_init(gpio, GPIO_PIN_13, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    /* I2S_DI (Data Input) on GPIO_PIN_10 */
    bflb_gpio_init(gpio, GPIO_PIN_10, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    /* I2S_DO (Data Output) on GPIO_PIN_11 */
    bflb_gpio_init(gpio, GPIO_PIN_11, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    /* I2S_BCLK (Bit Clock) on GPIO_PIN_20 */
    bflb_gpio_init(gpio, GPIO_PIN_20, GPIO_FUNC_I2S | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    
    /* MCLK CLKOUT on GPIO_PIN_14 */
    bflb_gpio_init(gpio, GPIO_PIN_14, GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    
    LOG_I("I2S GPIO configured\r\n");
}

// Initialize MCLK output
void init_mclk(void)
{
    GLB_Set_I2S_CLK(ENABLE, 2, GLB_I2S_DI_SEL_I2S_DI_INPUT, GLB_I2S_DO_SEL_I2S_DO_OUTPT);
    GLB_Set_Chip_Clock_Out2_Sel(GLB_CHIP_CLK_OUT_2_I2S_REF_CLK);
    LOG_I("MCLK configured\r\n");
}

// Initialize I2S interface
void init_i2s(void)
{
    struct bflb_i2s_config_s i2s_cfg = {
        .bclk_freq_hz = 16000 * 32 * 2,  // bclk = sample_rate * frame_width * channels (MUST be 32!)
        .role = I2S_ROLE_MASTER,
        .format_mode = I2S_MODE_LEFT_JUSTIFIED,
        .channel_mode = I2S_CHANNEL_MODE_NUM_2,  // Stereo
        .frame_width = I2S_SLOT_WIDTH_32,   // 32-bit frame (same as working project!)
        .data_width = I2S_SLOT_WIDTH_16,    // 16-bit data
        .fs_offset_cycle = 0,
        .tx_fifo_threshold = 0,
        .rx_fifo_threshold = 0,
    };

    i2s0 = bflb_device_get_by_name("i2s0");
    bflb_i2s_init(i2s0, &i2s_cfg);

    // Enable RX DMA
    bflb_i2s_link_rxdma(i2s0, true);

    LOG_I("I2S initialized\r\n");
}

// Initialize DMA for recording
void init_dma_rx(void)
{
    struct bflb_dma_channel_config_s rx_config = {
        .direction = DMA_PERIPH_TO_MEMORY,
        .src_req = DMA_REQUEST_I2S_RX,
        .dst_req = DMA_REQUEST_NONE,
        .src_addr_inc = DMA_ADDR_INCREMENT_DISABLE,
        .dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE,
        .src_burst_count = DMA_BURST_INCR1,
        .dst_burst_count = DMA_BURST_INCR1,
        .src_width = DMA_DATA_WIDTH_16BIT,
        .dst_width = DMA_DATA_WIDTH_16BIT,
    };

    dma0_ch0 = bflb_device_get_by_name("dma0_ch0");
    bflb_dma_channel_init(dma0_ch0, &rx_config);
    bflb_dma_channel_irq_attach(dma0_ch0, dma0_ch0_isr, NULL);

    LOG_I("DMA RX initialized\r\n");
}

// Initialize DMA for playback
void init_dma_tx(void)
{
    struct bflb_dma_channel_config_s tx_config = {
        .direction = DMA_MEMORY_TO_PERIPH,
        .src_req = DMA_REQUEST_NONE,
        .dst_req = DMA_REQUEST_I2S_TX,
        .src_addr_inc = DMA_ADDR_INCREMENT_ENABLE,
        .dst_addr_inc = DMA_ADDR_INCREMENT_DISABLE,
        .src_burst_count = DMA_BURST_INCR1,
        .dst_burst_count = DMA_BURST_INCR1,
        .src_width = DMA_DATA_WIDTH_16BIT,
        .dst_width = DMA_DATA_WIDTH_16BIT,
    };

    dma0_ch1 = bflb_device_get_by_name("dma0_ch1");
    bflb_dma_channel_init(dma0_ch1, &tx_config);
    // Interrupt will be attached when playing

    LOG_I("DMA TX initialized\r\n");
}

// Set I2S sample rate (for switching between recording and playback rates)
void set_i2s_sample_rate(uint32_t sample_rate)
{
    LOG_I("Setting I2S sample rate to %d Hz\r\n", sample_rate);

    // Disable I2S first
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);

    // Reconfigure I2S with new sample rate
    struct bflb_i2s_config_s i2s_cfg = {
        .bclk_freq_hz = sample_rate * 32 * 2,  // bclk = sample_rate * frame_width * channels
        .role = I2S_ROLE_MASTER,
        .format_mode = I2S_MODE_LEFT_JUSTIFIED,
        .channel_mode = I2S_CHANNEL_MODE_NUM_2,  // Stereo
        .frame_width = I2S_SLOT_WIDTH_32,
        .data_width = I2S_SLOT_WIDTH_16,
        .fs_offset_cycle = 0,
        .tx_fifo_threshold = 0,
        .rx_fifo_threshold = 0,
    };

    bflb_i2s_init(i2s0, &i2s_cfg);

    // Re-link DMA after I2S re-init (I2S init may reset DMA link)
    bflb_i2s_link_txdma(i2s0, true);
    bflb_i2s_link_rxdma(i2s0, true);
}

// Switch ES8388 mode
void switch_es8388_mode(ES8388_Work_Mode mode)
{
    if (current_es8388_mode == mode) {
        return;  // Already in this mode
    }

    LOG_I("Switching ES8388 to %s mode\r\n",
          mode == ES8388_RECORDING_MODE ? "RECORDING" :
          mode == ES8388_PLAY_BACK_MODE ? "PLAYBACK" : "CODEC");

    ES8388_Cfg_Type es8388_cfg = {
        .work_mode = mode,
        .role = ES8388_SLAVE,
        .mic_input_mode = ES8388_DIFF_ENDED_MIC,
        .mic_pga = ES8388_MIC_PGA_12DB,      // Balanced gain for good sensitivity with acceptable noise
        .i2s_frame = ES8388_LEFT_JUSTIFY_FRAME,
        .data_width = ES8388_DATA_LEN_16
    };

    ES8388_Init(&es8388_cfg);
    current_es8388_mode = mode;

    // Small delay to let codec stabilize
    vTaskDelay(pdMS_TO_TICKS(50));
}

// Play audio through speaker
int play_audio(uint8_t *pcm_data, uint32_t pcm_len)
{
    LOG_I("Playing %d bytes of audio\r\n", pcm_len);

    // Stop any ongoing recording
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);

    // Switch to playback mode
    switch_es8388_mode(ES8388_PLAY_BACK_MODE);

    // Enable I2S TX DMA
    bflb_i2s_link_txdma(i2s0, true);

    // Setup DMA transfer
    static struct bflb_dma_channel_lli_pool_s tx_llipool[100];
    struct bflb_dma_channel_lli_transfer_s transfer;

    transfer.src_addr = (uint32_t)pcm_data;
    transfer.dst_addr = (uint32_t)DMA_ADDR_I2S_TDR;
    transfer.nbytes = pcm_len;

    playback_complete = false;

    bflb_dma_channel_irq_attach(dma0_ch1, dma0_ch1_isr, NULL);

    uint32_t num = bflb_dma_channel_lli_reload(dma0_ch1, tx_llipool, 100, &transfer, 1);
    bflb_dma_channel_lli_link_head(dma0_ch1, tx_llipool, num);
    bflb_dma_channel_start(dma0_ch1);

    // Enable I2S TX
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, I2S_CMD_DATA_ENABLE_TX);

    // Wait for playback to complete
    uint32_t timeout = (pcm_len * 1000) / (AUDIO_SAMPLE_RATE * 2) + 1000;  // Calculate timeout based on audio length
    uint32_t wait_time = 0;

    while (!playback_complete && wait_time < timeout) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time += 100;
    }

    if (!playback_complete) {
        LOG_W("Playback timeout!\r\n");
        bflb_dma_channel_stop(dma0_ch1);
    }

    // Disable I2S TX
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);

    // Switch back to recording mode
    switch_es8388_mode(ES8388_RECORDING_MODE);

    // Re-enable I2S RX DMA
    bflb_i2s_link_rxdma(i2s0, true);

    LOG_I("Playback finished\r\n");
    return 0;
}

// Real-time recording with streaming to WhisperLive
// Returns transcribed text (caller must free) or NULL on failure
char* record_and_transcribe_realtime(uint32_t max_duration_ms)
{
    LOG_I("Starting real-time recording...\r\n");

    // Buffer for streaming chunks (250ms chunks to save memory)
    #define CHUNK_DURATION_MS 250
    #define CHUNK_SIZE (AUDIO_SAMPLE_RATE * 2 * 2 * CHUNK_DURATION_MS / 1000)  // 16-bit stereo = 16KB
    #define SILENCE_THRESHOLD 150  // Energy threshold for silence detection (lowered)
    #define SPEECH_THRESHOLD 180   // Energy threshold for speech detection
    #define SILENCE_CHUNKS_TO_STOP 5  // Stop after 5 consecutive silent chunks (1.25 seconds at 250ms)
    #define MAX_SILENCE_BEFORE_SPEECH 12  // Stop if no speech detected after 3 seconds

    // Use static buffer to avoid malloc
    static uint8_t chunk_buffer[CHUNK_SIZE];

    // Transcription buffer
    static char transcription_buffer[2048];
    transcription_buffer[0] = '\0';

    // Setup DMA for recording
    struct bflb_dma_channel_config_s dma_config;
    dma_config.direction = DMA_PERIPH_TO_MEMORY;
    dma_config.src_req = DMA_REQUEST_I2S_RX;
    dma_config.dst_req = DMA_REQUEST_NONE;
    dma_config.src_addr_inc = DMA_ADDR_INCREMENT_DISABLE;
    dma_config.dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE;
    dma_config.src_burst_count = DMA_BURST_INCR1;  // MUST be INCR1, not INCR4!
    dma_config.dst_burst_count = DMA_BURST_INCR1;  // MUST be INCR1, not INCR4!
    dma_config.src_width = DMA_DATA_WIDTH_16BIT;
    dma_config.dst_width = DMA_DATA_WIDTH_16BIT;
    bflb_dma_channel_init(dma0_ch0, &dma_config);

    uint32_t total_time = 0;
    uint32_t chunk_count = 0;
    bool speech_started = true;  // Assume speech already started from trigger detection
    uint32_t silence_count = 0;

    static struct bflb_dma_channel_lli_pool_s rx_llipool[20];
    struct bflb_dma_channel_lli_transfer_s transfer;
    
    // Send the trigger audio
    // Previously we tried to trim silence, but this was too aggressive and cut off speech
    // Now we send the full trigger buffer to ensure no speech is lost
    if (has_trigger_audio && trigger_audio_len > 0) {
        LOG_I("Sending trigger audio (%d bytes)...\r\n", trigger_audio_len);
        uint32_t offset = 0;
        while (offset < trigger_audio_len) {
            uint32_t chunk_to_send = trigger_audio_len - offset;
            if (chunk_to_send > CHUNK_SIZE) {
                chunk_to_send = CHUNK_SIZE;
            }
            if (stt_send_audio_chunk(trigger_audio_buffer + offset, chunk_to_send) < 0) {
                LOG_E("Failed to send trigger audio chunk at offset %d\r\n", offset);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            offset += chunk_to_send;
        }
        LOG_I("Trigger audio sent (%d bytes, %.1f seconds)\r\n", 
              trigger_audio_len, 
              (float)trigger_audio_len / (AUDIO_SAMPLE_RATE * 2 * 2));
        
        has_trigger_audio = false;
    }

    // Send the overlap audio (recorded during WebSocket connection)
    // This contains the speech that continued after trigger detection
    if (audio_recorded_size > 0) {
        LOG_I("Sending overlap audio (%d bytes) in chunks...\r\n", audio_recorded_size);
        uint32_t offset = 0;
        while (offset < audio_recorded_size) {
            uint32_t chunk_to_send = audio_recorded_size - offset;
            if (chunk_to_send > CHUNK_SIZE) {
                chunk_to_send = CHUNK_SIZE;
            }
            if (stt_send_audio_chunk(audio_buffer + offset, chunk_to_send) < 0) {
                LOG_E("Failed to send overlap audio chunk at offset %d\r\n", offset);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));  // Small delay to avoid overwhelming TCP buffer
            offset += chunk_to_send;
        }
        audio_recorded_size = 0;  // Clear
        LOG_I("Overlap audio sent\r\n");
    }

    // Now start real-time recording
    // Clear FIFO and start fresh recording
    bflb_i2s_feature_control(i2s0, I2S_CMD_CLEAR_RX_FIFO, 0);
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, I2S_CMD_DATA_ENABLE_RX);

    LOG_I("Real-time recording started\r\n");

    // Main recording loop
    while (total_time < max_duration_ms) {
        // Setup DMA transfer for this chunk
        memset(chunk_buffer, 0, CHUNK_SIZE);
        transfer.src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
        transfer.dst_addr = (uint32_t)chunk_buffer;
        transfer.nbytes = CHUNK_SIZE;

        uint32_t num = bflb_dma_channel_lli_reload(dma0_ch0, rx_llipool, 20, &transfer, 1);
        bflb_dma_channel_lli_link_head(dma0_ch0, rx_llipool, num);
        bflb_dma_channel_start(dma0_ch0);

        // Wait for chunk to complete
        vTaskDelay(pdMS_TO_TICKS(CHUNK_DURATION_MS));

        // Stop DMA
        bflb_dma_channel_stop(dma0_ch0);

        // Invalidate cache to ensure CPU reads fresh data from PSRAM
        bflb_l1c_dcache_invalidate_range((void*)chunk_buffer, CHUNK_SIZE);

        chunk_count++;
        total_time += CHUNK_DURATION_MS;

        // Calculate energy
        int16_t *samples = (int16_t*)chunk_buffer;
        uint32_t num_samples = CHUNK_SIZE / 2;
        int64_t sum = 0;
        for (uint32_t i = 0; i < num_samples; i++) {
            int32_t s = samples[i];
            sum += (s > 0) ? s : -s;
        }
        uint32_t avg_energy = (uint32_t)(sum / num_samples);

        // Batch sending logic: Accumulate 4 chunks (1 second) before sending
        // This reduces server inference frequency and improves speed
        #define BATCH_SIZE 4
        static uint8_t *batch_buffer = NULL;
        static uint32_t batch_len = 0;
        
        if (batch_buffer == NULL) {
            batch_buffer = pvPortMalloc(CHUNK_SIZE * BATCH_SIZE);
        }
        
        if (batch_buffer) {
            memcpy(batch_buffer + batch_len, chunk_buffer, CHUNK_SIZE);
            batch_len += CHUNK_SIZE;
            
            // Send only when batch is full or recording is ending
            if (batch_len >= CHUNK_SIZE * BATCH_SIZE) {
                int sent = stt_send_audio_chunk(batch_buffer, batch_len);
                if (sent < 0) {
                    LOG_E("Failed to send batch audio\r\n");
                    break;
                }
                LOG_I("Sent batch audio (%d bytes)\r\n", batch_len);
                batch_len = 0;
                vTaskDelay(pdMS_TO_TICKS(5)); // Small delay after batch send
            }
        } else {
            // Fallback if malloc failed
            int sent = stt_send_audio_chunk(chunk_buffer, CHUNK_SIZE);
            if (sent < 0) {
                LOG_E("Failed to send chunk %d\r\n", chunk_count);
                break;
            }
        }

        // Check for speech/silence
        if (avg_energy > SPEECH_THRESHOLD) {
            speech_started = true;
            silence_count = 0;
            LOG_I("Chunk %d: energy=%d [SPEECH]\r\n", chunk_count, avg_energy);
        } else if (avg_energy > SILENCE_THRESHOLD) {
            // Medium energy - don't reset silence count, let it accumulate
            // Only reset if we haven't started speaking yet
            if (!speech_started) {
                silence_count = 0;
            }
            LOG_I("Chunk %d: energy=%d [medium]%s\r\n", chunk_count, avg_energy, 
                  speech_started ? "" : " (pre-speech)");
        } else {
            silence_count++;
            LOG_I("Chunk %d: energy=%d [silence %d/%d]\r\n", chunk_count, avg_energy, silence_count, SILENCE_CHUNKS_TO_STOP);

            if (speech_started && silence_count >= SILENCE_CHUNKS_TO_STOP) {
                LOG_I("Speech ended, stopping recording\r\n");
                break;
            }

            if (!speech_started && silence_count >= MAX_SILENCE_BEFORE_SPEECH) {
                LOG_I("No speech detected, stopping recording\r\n");
                break;
            }
        }

        // Check for transcription (non-blocking)
        char temp_text[512];
        int received = stt_recv_transcription(temp_text, sizeof(temp_text), 50);
        if (received > 0 && strlen(temp_text) > 0) {
            LOG_I("Transcription: %s\r\n", temp_text);
            // Replace (not append) with latest transcription
            // WhisperLive sends progressive updates, we only want the latest
            strncpy(transcription_buffer, temp_text, sizeof(transcription_buffer) - 1);
            transcription_buffer[sizeof(transcription_buffer) - 1] = '\0';
        }
    }

    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);
    LOG_I("Recording done (%d chunks, %d ms)\r\n", chunk_count, total_time);

    // Wait for final transcription
    // Adaptive timeout based on recording duration
    // Base wait: 20 seconds (maximized for CPU inference), plus 2 seconds for every second of recording
    int wait_seconds = 20 + (total_time / 1000) * 2;
    if (wait_seconds < 20) wait_seconds = 20;   // Minimum 20 seconds
    if (wait_seconds > 60) wait_seconds = 60; // Maximum 60 seconds
    
    LOG_I("Waiting for transcription (timeout: %d s)...\r\n", wait_seconds);
    char final_text[512];
    for (int i = 0; i < wait_seconds; i++) {
        int final_received = stt_recv_transcription(final_text, sizeof(final_text), 1000);
        if (final_received > 0 && strlen(final_text) > 0) {
            LOG_I("Final: %s\r\n", final_text);
            // Replace (not append) with final transcription
            strncpy(transcription_buffer, final_text, sizeof(transcription_buffer) - 1);
            transcription_buffer[sizeof(transcription_buffer) - 1] = '\0';
            break;
        }
        LOG_I("Waiting... (%d/%d)\r\n", i + 1, wait_seconds);
    }

    // Disconnect from STT service
    stt_disconnect();

    if (strlen(transcription_buffer) > 0) {
        char *result = pvPortMalloc(strlen(transcription_buffer) + 1);
        if (result) {
            strcpy(result, transcription_buffer);
        }
        return result;
    }

    return NULL;
}

// Start recording
void start_recording(void)
{
    static struct bflb_dma_channel_lli_pool_s rx_llipool[100]; // Larger pool for bigger buffer
    struct bflb_dma_channel_lli_transfer_s transfers[1];

    recording_complete = false;
    audio_recorded_size = 0;
    audio_write_pos = 0;
    
    // Allocate buffer if not already done
    if (audio_buffer == NULL) {
        audio_buffer = pvPortMalloc(AUDIO_BUFFER_SIZE);
        if (audio_buffer == NULL) {
            LOG_E("Failed to allocate audio buffer (%d bytes)\r\n", AUDIO_BUFFER_SIZE);
            return;
        }
        memset(audio_buffer, 0, AUDIO_BUFFER_SIZE);  // Clear buffer
        LOG_I("Allocated audio buffer at %p\r\n", audio_buffer);
    } else {
        // Clear buffer for reuse
        memset(audio_buffer, 0, AUDIO_BUFFER_SIZE);
    }

    // Initialize VAD
    vad_init(&vad_state);

    transfers[0].src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
    transfers[0].dst_addr = (uint32_t)audio_buffer;
    transfers[0].nbytes = AUDIO_BUFFER_SIZE;

    // Use the pool and link head
    uint32_t num = bflb_dma_channel_lli_reload(dma0_ch0, rx_llipool, 100, transfers, 1);
    bflb_dma_channel_lli_link_head(dma0_ch0, rx_llipool, num);
    bflb_dma_channel_start(dma0_ch0);

    // Enable I2S RX
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, I2S_CMD_DATA_ENABLE_RX);

    LOG_I("Recording started (max %d seconds)\r\n", AUDIO_MAX_RECORD_TIME);
}

// Check VAD during recording
// Returns: 0 = continue, 1 = speech ended, 2 = buffer full
int check_vad_during_recording(void)
{
    // Get current DMA position by checking how much data has been transferred
    // This is an approximation - we check the buffer content
    static uint32_t last_checked_pos = 0;

    // Estimate current write position based on time elapsed
    // Since we can't easily get DMA counter, we'll process chunks as they come
    // For simplicity, let's check every 500ms worth of data

    uint32_t check_interval = AUDIO_SAMPLE_RATE * 2 * 2 * 0.5; // 500ms of stereo data

    // Check if we have new data to process
    if (recording_complete) {
        return 2; // Buffer full
    }

    // Calculate approximate current position based on elapsed time
    // This is a simplified approach - in production you'd use DMA counter
    static uint32_t start_tick = 0;
    if (start_tick == 0) {
        start_tick = xTaskGetTickCount();
        return 0;
    }

    uint32_t elapsed_ms = (xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS;
    uint32_t estimated_pos = (AUDIO_SAMPLE_RATE * 2 * 2 * elapsed_ms) / 1000;  // stereo

    if (estimated_pos < last_checked_pos + check_interval) {
        return 0; // Not enough new data yet
    }

    // Process VAD on the new data
    if (estimated_pos > AUDIO_BUFFER_SIZE) {
        estimated_pos = AUDIO_BUFFER_SIZE;
    }

    // Convert stereo to mono and process VAD
    int16_t *samples = (int16_t*)(audio_buffer + last_checked_pos);
    uint32_t num_samples = (estimated_pos - last_checked_pos) / 2; // 16-bit samples
    uint32_t num_frames = num_samples / 2; // Stereo frames

    // Process in chunks to avoid stack overflow
    for (uint32_t i = 0; i < num_frames; i += VAD_FRAME_SIZE) {
        uint32_t frame_len = (i + VAD_FRAME_SIZE <= num_frames) ? VAD_FRAME_SIZE : (num_frames - i);

        // Extract left channel only for VAD
        int16_t mono_frame[VAD_FRAME_SIZE];
        for (uint32_t j = 0; j < frame_len; j++) {
            mono_frame[j] = samples[(i + j) * 2];  // Left channel only
        }
        vad_process_frame(&vad_state, mono_frame, frame_len);
    }

    last_checked_pos = estimated_pos;

    // Check if speech ended
    if (vad_speech_ended(&vad_state) && vad_has_speech(&vad_state)) {
        // Stop DMA
        bflb_dma_channel_stop(dma0_ch0);
        recording_complete = true;
        audio_recorded_size = estimated_pos;
        LOG_I("Speech ended via VAD at %d ms (%d bytes)\r\n", elapsed_ms, audio_recorded_size);
        return 1;
    }

    return 0; // Continue recording
}

// WiFi firmware task initialization
int wifi_start_firmware_task(void)
{
    LOG_I("Starting wifi...\r\n");

    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);
    GLB_Set_EM_Sel(GLB_WRAM160KB_EM0KB);

    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    LOG_I("PHY RF init success!\r\n");

    extern void interrupt0_handler(void);
    bflb_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    bflb_irq_enable(WIFI_IRQn);

    extern void wifi_main(void *param);
    xTaskCreate(wifi_main, (char*)"fw", WIFI_STACK_SIZE, NULL, TASK_PRIORITY_FW, &wifi_fw_task);
    return 0;
}

// WiFi event handler
void wifi_event_handler(uint32_t code)
{
    sta_connect_status = code;
    
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE:
            LOG_I("[WiFi] Init done\r\n");
            wifi_mgmr_init(&conf);
            break;
            
        case CODE_WIFI_ON_MGMR_DONE:
            LOG_I("[WiFi] Manager done\r\n");
            break;
            
        case CODE_WIFI_ON_CONNECTED:
            LOG_I("[WiFi] Connected!\r\n");
            break;
            
        case CODE_WIFI_ON_GOT_IP:
        {
            uint32_t ipv4_addr;
            wifi_sta_ip4_addr_get(&ipv4_addr, NULL, NULL, NULL);
            LOG_I("[WiFi] Got IP: %s\r\n", inet_ntoa(ipv4_addr));
            break;
        }
            
        case CODE_WIFI_ON_DISCONNECT:
            LOG_I("[WiFi] Disconnected\r\n");
            break;
            
        default:
            break;
    }
}

// Simple HTTP GET test
void test_http_connection(void)
{
    LOG_I("Testing HTTP connection...\r\n");
    LOG_I("WhisperLive WebSocket: %s\r\n", WHISPERLIVE_WS_URL);
    
    // Extract hostname and path from URL
    // For http://192.168.1.151:9977/v1/audio/transcriptions
    const char* host = "192.168.1.151";
    int port = 9977;
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_I("Socket creation failed\r\n");
        return;
    }
    
    // Connect
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(host, &server_addr.sin_addr);
    
    LOG_I("Connecting to %s:%d...\r\n", host, port);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_I("Connection failed\r\n");
        close(sock);
        return;
    }

    LOG_I("Connected! HTTP test successful\r\n");
    close(sock);
}

// Voice activity detection threshold for auto-start
#define VOICE_ENERGY_THRESHOLD 2000  // Energy threshold to trigger recording (raised to avoid noise triggers)

// Overlap buffer - reuse audio_buffer to save memory
// 500ms buffer to capture speech while sending trigger audio
#define OVERLAP_BUFFER_MS 500
#define OVERLAP_BUFFER_SIZE AUDIO_BUFFER_SIZE  // Same as audio_buffer (32KB)
// Note: overlap_audio is stored in audio_buffer (reusing the buffer)

// Listen for voice activity and continue recording if detected
// This function now handles the complete recording flow to avoid gaps
bool listen_and_record_if_voice(uint32_t listen_duration_ms, char **out_transcription)
{
    static struct bflb_dma_channel_lli_pool_s listen_llipool[20];
    struct bflb_dma_channel_lli_transfer_s transfer;

    *out_transcription = NULL;

    // Allocate trigger buffer if not already done
    if (trigger_audio_buffer == NULL) {
        trigger_audio_buffer = pvPortMalloc(TRIGGER_BUFFER_SIZE);
        if (trigger_audio_buffer == NULL) {
            LOG_E("Failed to allocate trigger buffer (%d bytes)\r\n", TRIGGER_BUFFER_SIZE);
            return false;
        }
        memset(trigger_audio_buffer, 0, TRIGGER_BUFFER_SIZE);  // Clear buffer
        LOG_I("Allocated trigger buffer at %p\r\n", trigger_audio_buffer);
    }

    // Use the trigger buffer for initial listening
    trigger_audio_len = TRIGGER_BUFFER_SIZE;
    
    // Clear buffer to avoid noise triggering VAD
    memset(trigger_audio_buffer, 0, TRIGGER_BUFFER_SIZE);
    
    // CRITICAL: Clean cache to ensure zeros are written to PSRAM
    // Without this, CPU cache might write back old data AFTER DMA writes new data
    bflb_l1c_dcache_clean_range((void*)trigger_audio_buffer, TRIGGER_BUFFER_SIZE);

    // Setup DMA
    struct bflb_dma_channel_config_s dma_config;
    dma_config.direction = DMA_PERIPH_TO_MEMORY;
    dma_config.src_req = DMA_REQUEST_I2S_RX;
    dma_config.dst_req = DMA_REQUEST_NONE;
    dma_config.src_addr_inc = DMA_ADDR_INCREMENT_DISABLE;
    dma_config.dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE;
    dma_config.src_burst_count = DMA_BURST_INCR1;
    dma_config.dst_burst_count = DMA_BURST_INCR1;
    dma_config.src_width = DMA_DATA_WIDTH_16BIT;
    dma_config.dst_width = DMA_DATA_WIDTH_16BIT;
    bflb_dma_channel_init(dma0_ch0, &dma_config);

    // Setup transfer to trigger buffer
    transfer.src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
    transfer.dst_addr = (uint32_t)trigger_audio_buffer;
    transfer.nbytes = trigger_audio_len;

    // Start DMA for listening
    uint32_t num = bflb_dma_channel_lli_reload(dma0_ch0, listen_llipool, 20, &transfer, 1);
    bflb_dma_channel_lli_link_head(dma0_ch0, listen_llipool, num);

    // Clear I2S RX FIFO before starting
    bflb_i2s_feature_control(i2s0, I2S_CMD_CLEAR_RX_FIFO, 0);

    // Ensure TX is disabled to avoid conflicts
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0); // Disable all first
    bflb_i2s_link_txdma(i2s0, false); // Disable TX DMA

    // Enable I2S RX, then start DMA
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, I2S_CMD_DATA_ENABLE_RX);
    bflb_dma_channel_start(dma0_ch0);

    // Wait for trigger buffer to fill
    vTaskDelay(pdMS_TO_TICKS(listen_duration_ms));

    // Stop DMA for trigger buffer
    bflb_dma_channel_stop(dma0_ch0);

    // Invalidate cache to ensure CPU reads fresh data from PSRAM
    bflb_l1c_dcache_invalidate_range((void*)trigger_audio_buffer, trigger_audio_len);

    // Calculate energy
    int16_t *samples = (int16_t*)trigger_audio_buffer;
    uint32_t num_samples = trigger_audio_len / 2;
    uint64_t sum_squares = 0;

    for (uint32_t i = 0; i < num_samples; i++) {
        int32_t sample = samples[i];
        sum_squares += (uint64_t)(sample * sample);
    }

    uint32_t energy = 0;
    if (num_samples > 0) {
        energy = (uint32_t)sqrt((double)sum_squares / num_samples);
    }

    // Check if voice detected
    bool voice_detected = (energy > VOICE_ENERGY_THRESHOLD);
    if (!voice_detected) {
        // No voice - stop I2S RX now
        bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);
        LOG_I("Energy: %d (threshold: %d)\r\n", energy, VOICE_ENERGY_THRESHOLD);
        return false;
    }

    LOG_I("Energy: %d [VOICE DETECTED! > %d]\r\n", energy, VOICE_ENERGY_THRESHOLD);


    // Voice detected!
    // CRITICAL: Start recording to audio_buffer IMMEDIATELY while we connect
    // This captures the speech that continues after trigger detection
    
    // Allocate audio_buffer if not already done (to avoid NULL pointer crash)
    if (audio_buffer == NULL) {
        audio_buffer = pvPortMalloc(AUDIO_BUFFER_SIZE);
        if (audio_buffer == NULL) {
            LOG_E("Failed to allocate audio buffer (%d bytes)\r\n", AUDIO_BUFFER_SIZE);
            bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);
            return false;
        }
        LOG_I("Allocated audio buffer at %p\r\n", audio_buffer);
    }
    
    // Clear buffer to avoid sending old audio from previous session
    memset(audio_buffer, 0, AUDIO_BUFFER_SIZE);

    // Setup DMA to record to audio_buffer (256KB = 4 seconds) while connecting
    transfer.src_addr = (uint32_t)DMA_ADDR_I2S_RDR;
    transfer.dst_addr = (uint32_t)audio_buffer;
    transfer.nbytes = AUDIO_BUFFER_SIZE;

    num = bflb_dma_channel_lli_reload(dma0_ch0, listen_llipool, 20, &transfer, 1);
    bflb_dma_channel_lli_link_head(dma0_ch0, listen_llipool, num);
    bflb_dma_channel_start(dma0_ch0);

    LOG_I("Recording to overlap buffer while connecting...\r\n");

    // Connect to WhisperLive (audio continues to record during this!)
    LOG_I("Connecting to WhisperLive...\r\n");
    stt_disconnect();  // Ensure clean state
    if (stt_connect() < 0) {
        LOG_E("Failed to connect to WhisperLive\r\n");
        bflb_dma_channel_stop(dma0_ch0);
        bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);
        
        // Critical: Ensure recording is ready for next attempt
        switch_es8388_mode(ES8388_RECORDING_MODE);
        bflb_i2s_link_rxdma(i2s0, true);
        
        return false;
    }

    // Stop the overlap recording (we have audio_buffer filled with 500ms of continued speech)
    bflb_dma_channel_stop(dma0_ch0);
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);

    // Invalidate cache for overlap buffer
    bflb_l1c_dcache_invalidate_range((void*)audio_buffer, AUDIO_BUFFER_SIZE);

    // Calculate actual recorded size based on connection time
    // Stop happens right after stt_connect() returns, estimate ~1-2 seconds
    // Each second = 16000 * 2 * 2 = 64000 bytes
    // Conservative: use 2 seconds = 128KB
    uint32_t estimated_connection_time_ms = 2000;  // Typical connection time
    audio_recorded_size = (AUDIO_SAMPLE_RATE * 2 * 2 * estimated_connection_time_ms) / 1000;
    if (audio_recorded_size > AUDIO_BUFFER_SIZE) {
        audio_recorded_size = AUDIO_BUFFER_SIZE;
    }
    LOG_I("Overlap buffer size: %d bytes (%.1f seconds)\r\n", 
          audio_recorded_size, (float)audio_recorded_size / (AUDIO_SAMPLE_RATE * 2 * 2));
    
    // Mark trigger audio for sending
    has_trigger_audio = true;
    
    // Start continuous real-time recording (will send trigger audio + overlap + realtime)
    *out_transcription = record_and_transcribe_realtime(30000);
    return true;
}

// Voice assistant task
void voice_assistant_task(void *pvParameters)
{
    LOG_I("Voice Assistant Task Started!\r\n");
    
    // Wait for WiFi manager to be ready
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Connect to WiFi
    LOG_I("Connecting to WiFi: %s\r\n", WIFI_SSID);
    if (wifi_sta_connect(WIFI_SSID, WIFI_PASSWORD, NULL, NULL, 0, 0, 0, 1) != 0) {
        LOG_I("WiFi connect failed!\r\n");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Wait for connection (max 30 seconds)
    for (int i = 0; i < 300; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        
        if (sta_connect_status == CODE_WIFI_ON_GOT_IP) {
            LOG_I("WiFi connection successful!\r\n");
            break;
        }
        
        if (sta_connect_status == CODE_WIFI_ON_DISCONNECT) {
            LOG_I("WiFi connection failed - disconnected\r\n");
            while(1) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Step 2: Test HTTP client
    LOG_I("\r\n=== Step 2: Testing HTTP Client ===\r\n");
    test_http_connection();

    // Step 2.5: Initialize and connect to WhisperLive
    LOG_I("\r\n=== Step 2.5: Initializing WhisperLive STT ===\r\n");
    if (stt_init(WHISPERLIVE_WS_URL) < 0) {
        LOG_E("Failed to initialize WhisperLive client\r\n");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (stt_connect() < 0) {
        LOG_E("Failed to connect to WhisperLive server\r\n");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    LOG_I("WhisperLive STT connected successfully!\r\n");

    //Step 3: Initialize ES8388 audio codec
    LOG_I("\r\n=== Step 3: Initializing ES8388 Audio Codec ===\r\n");
    
    // First, initialize I2C GPIO pins (critical for I2C communication)
    LOG_I("Configuring I2C GPIO pins...\r\n");
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
    
    /* I2C0_SCL on GPIO_PIN_0 */
    bflb_gpio_init(gpio, GPIO_PIN_0, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    /* I2C0_SDA on GPIO_PIN_1 */
    bflb_gpio_init(gpio, GPIO_PIN_1, GPIO_FUNC_I2C0 | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_2);
    LOG_I("I2C GPIO configured: SCL=PIN_0, SDA=PIN_1\r\n");
    
    ES8388_Cfg_Type es8388_cfg = {
        .work_mode = ES8388_RECORDING_MODE,  // Recording mode for microphone
        .role = ES8388_SLAVE,                // I2S slave mode
        .mic_input_mode = ES8388_DIFF_ENDED_MIC,  // Differential input
        .mic_pga = ES8388_MIC_PGA_18DB,      // 18dB (same as working project!)
        .i2s_frame = ES8388_LEFT_JUSTIFY_FRAME,   // Left Justified (Matches I2S controller)
        .data_width = ES8388_DATA_LEN_16     // 16-bit audio
    };
    
    LOG_I("Initializing ES8388...\r\n");
    ES8388_Init(&es8388_cfg);
    LOG_I("ES8388 initialized successfully!\r\n");
    
    // Dump registers for verification
    LOG_I("ES8388 Register Dump:\r\n");
    ES8388_Reg_Dump();
    
    // Step 4: Initialize I2S + DMA
    LOG_I("\r\n=== Step 4: Initializing I2S + DMA ===\r\n");
    
    LOG_I("Configuring I2S GPIO...\r\n");
    init_i2s_gpio();
    
    LOG_I("Initializing MCLK...\r\n");
    init_mclk();
    
    LOG_I("Initializing I2S interface...\r\n");
    init_i2s();

    LOG_I("Initializing DMA RX...\r\n");
    init_dma_rx();

    LOG_I("Initializing DMA TX...\r\n");
    init_dma_tx();

    LOG_I("I2S + DMA initialized successfully!\r\n");

    // Warmup: discard initial audio samples (hardware settling noise)
    LOG_I("Warming up audio hardware (2 seconds)...\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Step 5: Test recording with voice detection
    LOG_I("\r\n=== Step 5: Testing Audio Recording ===\r\n");
    LOG_I("Waiting for voice activity to start recording...\r\n");
    LOG_I("(Energy threshold: %d - speak to trigger recording)\r\n", VOICE_ENERGY_THRESHOLD);

    // Wait for voice activity and record
    char *text = NULL;
    while (!listen_and_record_if_voice(TRIGGER_BUFFER_MS, &text)) {
        // Keep listening until voice is detected
    }

    LOG_I("*** Recording complete ***\r\n");

        if (text) {
            LOG_I("STT Result: \"%s\"\r\n", text);

            // Step 7: DeepSeek Integration
            LOG_I("\r\n=== Step 7: DeepSeek API Integration ===\r\n");
            char *ai_reply = deepseek_chat(text);

            if (ai_reply) {
                LOG_I("AI Reply: \"%s\"\r\n", ai_reply);

                // Step 8: TTS + Playback (Streaming)
                LOG_I("\r\n=== Step 8: Streaming TTS & Audio Playback ===\r\n");

                if (tts_synthesize_and_play_streaming(ai_reply) == 0) {
                    LOG_I("Streaming TTS playback complete\r\n");
                } else {
                    LOG_E("Streaming TTS failed\r\n");
                }

                vPortFree(ai_reply);
            } else {
                LOG_E("DeepSeek failed to reply\r\n");
            }

            vPortFree(text);
    } else {
        LOG_E("Real-time transcription failed\r\n");
    }

    // Main loop - Voice Assistant with auto-start
    LOG_I("\r\n=== Voice Assistant Auto-Start Loop ===\r\n");
    
    // Ensure no lingering connections from initialization
    stt_disconnect();
    
    LOG_I("Listening for voice activity...\r\n");

    while (1) {
        // Listen for voice activity
        LOG_I("Listening...\r\n");

        char *text = NULL;
        if (listen_and_record_if_voice(TRIGGER_BUFFER_MS, &text)) {  // Listen for voice, record if detected
            if (text && strlen(text) > 0) {
                LOG_I("STT: \"%s\"\r\n", text);

                // AI
                LOG_I("Sending to AI...\r\n");
                char *ai_reply = deepseek_chat(text);

                if (ai_reply) {
                    LOG_I("AI: \"%s\"\r\n", ai_reply);

                    // Streaming TTS + Playback
                    tts_synthesize_and_play_streaming(ai_reply);

                    vPortFree(ai_reply);
                }

                vPortFree(text);
            }

            LOG_I("Ready for next command...\r\n");
        }

        // Small delay before next listen cycle
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    board_init();

    LOG_I("=== AiPi Voice Assistant ===\r\n");
    LOG_I("Step-by-Step Implementation\r\n");
    LOG_I("Board initialized\r\n");

    // Initialize LwIP TCP/IP stack
    tcpip_init(NULL, NULL);
    
    // Start WiFi firmware
    wifi_start_firmware_task();

    // Create voice assistant task
    configASSERT((xTaskCreate(voice_assistant_task, "voice", 4096, NULL, 15, NULL) == pdPASS));

    LOG_I("Starting FreeRTOS scheduler...\r\n");
    vTaskStartScheduler();

    while (1) {
    }
}
