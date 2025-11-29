#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "log.h"
#include "cJSON.h"
#include "config.h"
#include "bsp_es8388.h"
#include "bflb_i2s.h"
#include "bflb_dma.h"
#include "bflb_l1c.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>

#define DBG_TAG "TTS"

// Streaming TTS buffer configuration
// Make buffer size a multiple of frame size (4 bytes per stereo sample)
#define TTS_CHUNK_SIZE (4 * 1024)        // 4KB per buffer (mono PCM) = 2048 samples (128ms at 16kHz)
#define TTS_STEREO_CHUNK_SIZE (8 * 1024) // 8KB stereo buffer = 2048 stereo frames
#define TTS_NUM_BUFFERS 2                // Double buffering
#define TTS_RECV_BUF_SIZE 2048           // Network receive buffer

// External I2S and DMA handles (defined in main.c)
extern struct bflb_device_s *i2s0;
extern struct bflb_device_s *dma0_ch0;

// External functions from main.c
extern void switch_es8388_mode(ES8388_Work_Mode mode);
extern void set_i2s_sample_rate(uint32_t sample_rate);

// Recording sample rate (for WhisperLive)
#define RECORDING_SAMPLE_RATE 16000

// DMA channel for TX playback (from main.c)
extern struct bflb_device_s *dma0_ch1;

// Static buffers for streaming playback (in SRAM to avoid DMA issues with PSRAM)
static int16_t stereo_buffer_0[TTS_STEREO_CHUNK_SIZE / 2] __attribute__((aligned(4)));
static int16_t stereo_buffer_1[TTS_STEREO_CHUNK_SIZE / 2] __attribute__((aligned(4)));
static int16_t *stereo_buffers[TTS_NUM_BUFFERS] = {stereo_buffer_0, stereo_buffer_1};

// DMA LLI pool
static struct bflb_dma_channel_lli_pool_s tx_llipool[20];

// DMA completion flag (for interrupt-based playback)
static volatile bool dma_transfer_done = false;

// DMA interrupt callback
static void tts_dma_isr(void *arg)
{
    dma_transfer_done = true;
}

// Convert mono to stereo in place (source buffer to destination buffer)
static void mono_to_stereo(int16_t *mono, int16_t *stereo, uint32_t mono_samples)
{
    for (uint32_t i = 0; i < mono_samples; i++) {
        stereo[i * 2] = mono[i];      // Left
        stereo[i * 2 + 1] = mono[i];  // Right
    }
}



// Non-blocking play buffer - starts DMA and returns immediately
static void play_buffer_non_blocking(int16_t *buffer, uint32_t len)
{
    // Reset completion flag
    dma_transfer_done = false;

    // Attach interrupt (must do this every time after mode switch)
    bflb_dma_channel_irq_attach(dma0_ch1, tts_dma_isr, NULL);

    // Flush cache
    bflb_l1c_dcache_clean_invalidate_range((void*)buffer, len);
    
    struct bflb_dma_channel_lli_transfer_s transfer;
    transfer.src_addr = (uint32_t)buffer;
    transfer.dst_addr = (uint32_t)DMA_ADDR_I2S_TDR;
    transfer.nbytes = len;

    uint32_t num = bflb_dma_channel_lli_reload(dma0_ch1, tx_llipool, 20, &transfer, 1);
    bflb_dma_channel_lli_link_head(dma0_ch1, tx_llipool, num);
    bflb_dma_channel_start(dma0_ch1);

    // Enable I2S TX (idempotent, safe to call multiple times)
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, I2S_CMD_DATA_ENABLE_TX);
}


// Parse URL to extract host, port, path
static int parse_tts_url(const char *url, char *host, int *port, char *path)
{
    // Format: http://host:port/path
    if (strncmp(url, "http://", 7) != 0) {
        return -1;
    }

    const char *host_start = url + 7;
    const char *port_start = strchr(host_start, ':');
    const char *path_start = strchr(host_start, '/');

    if (!port_start || !path_start) {
        return -1;
    }

    int host_len = port_start - host_start;
    strncpy(host, host_start, host_len);
    host[host_len] = '\0';

    *port = atoi(port_start + 1);
    strcpy(path, path_start);

    return 0;
}

// Streaming TTS synthesis and playback
int tts_synthesize_and_play_streaming(const char *text)
{
    if (!text || strlen(text) == 0) {
        LOG_E("Text is empty\r\n");
        return -1;
    }

    LOG_I("Streaming TTS: %s\r\n", text);

    int sockfd = -1;
    char *recv_buf = NULL;
    char *mono_buffer = NULL;
    int result = -1;

    // Allocate buffers (network buffers can use PSRAM)
    recv_buf = pvPortMalloc(TTS_RECV_BUF_SIZE);
    mono_buffer = pvPortMalloc(TTS_CHUNK_SIZE);

    if (!recv_buf || !mono_buffer) {
        LOG_E("Failed to allocate buffers\r\n");
        goto cleanup;
    }

    // Check DMA channel is available (initialized in main.c)
    if (!dma0_ch1) {
        LOG_E("DMA channel not initialized\r\n");
        goto cleanup;
    }

    // Stop any ongoing DMA transfer first
    bflb_dma_channel_stop(dma0_ch1);

    // Re-initialize DMA for TX (needed after sample rate change)
    struct bflb_dma_channel_config_s dma_config = {
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
    bflb_dma_channel_init(dma0_ch1, &dma_config);

    LOG_I("DMA TX configured for streaming\r\n");

    // Parse URL
    char host[64];
    char path[128];
    int port;

    if (parse_tts_url(TTS_API_URL, host, &port, path) < 0) {
        LOG_E("Failed to parse TTS URL\r\n");
        goto cleanup;
    }

    // Create JSON request body
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "text", text);
    cJSON_AddStringToObject(root, "format", TTS_FORMAT);
    cJSON_AddNumberToObject(root, "chunk_length", 200);
    cJSON_AddBoolToObject(root, "normalize", true);
    // Request 16kHz audio to match speaker hardware and avoid distortion
    cJSON_AddNumberToObject(root, "sample_rate", 16000);
    cJSON_AddNumberToObject(root, "mp3_bitrate", 192);  // Increased from 64 to 192 for better quality
    cJSON_AddNumberToObject(root, "opus_bitrate", -1000);
    cJSON_AddItemToObject(root, "references", cJSON_CreateArray());

    // Add reference_id if configured (for voice selection)
    #ifdef TTS_REFERENCE_ID
    cJSON_AddStringToObject(root, "reference_id", TTS_REFERENCE_ID);
    #endif

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!body) {
        LOG_E("Failed to create JSON body\r\n");
        goto cleanup;
    }

    int body_len = strlen(body);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_E("Failed to create socket\r\n");
        vPortFree(body);
        goto cleanup;
    }

    // Set socket timeout
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 60;
    tv_timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));

    // Connect
    struct hostent *server = gethostbyname(host);
    if (!server) {
        LOG_E("Failed to resolve host\r\n");
        vPortFree(body);
        goto cleanup;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    LOG_I("Connecting to %s:%d...\r\n", host, port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_E("Failed to connect\r\n");
        vPortFree(body);
        goto cleanup;
    }

    LOG_I("Connected! Sending request...\r\n");

    // Send HTTP request
    char request_header[512];
    int header_len = sprintf(request_header,
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, body_len);

    send(sockfd, request_header, header_len, 0);
    send(sockfd, body, body_len, 0);
    vPortFree(body);

    LOG_I("Request sent, waiting for response...\r\n");

    // Read HTTP headers first
    int header_end = 0;
    int total_header = 0;
    char header_buf[512];

    while (total_header < sizeof(header_buf) - 1) {
        int r = recv(sockfd, header_buf + total_header, 1, 0);
        if (r <= 0) break;
        total_header++;

        // Check for end of headers
        if (total_header >= 4 &&
            header_buf[total_header-4] == '\r' && header_buf[total_header-3] == '\n' &&
            header_buf[total_header-2] == '\r' && header_buf[total_header-1] == '\n') {
            header_end = 1;
            break;
        }
    }

    if (!header_end) {
        LOG_E("Failed to receive HTTP headers\r\n");
        goto cleanup;
    }

    header_buf[total_header] = '\0';
    LOG_I("HTTP Headers received\r\n");

    // Check if response uses chunked transfer encoding
    bool is_chunked = (strstr(header_buf, "chunked") != NULL);
    LOG_I("Transfer-Encoding: %s\r\n", is_chunked ? "chunked" : "identity");

    // If chunked, skip the first chunk size line (e.g., "4102c\r\n")
    int initial_chunk_size = 0;
    // If chunked, skip the first chunk size line (e.g., "4102c\r\n")
    if (is_chunked) {
        char chunk_line[32];
        int chunk_line_pos = 0;
        while (chunk_line_pos < sizeof(chunk_line) - 1) {
            int r = recv(sockfd, chunk_line + chunk_line_pos, 1, 0);
            if (r <= 0) break;
            if (chunk_line[chunk_line_pos] == '\n' && chunk_line_pos > 0 && chunk_line[chunk_line_pos-1] == '\r') {
                chunk_line[chunk_line_pos-1] = '\0';
                break;
            }
            chunk_line_pos++;
        }
        LOG_I("First chunk size: %s (hex)\r\n", chunk_line);
        initial_chunk_size = strtol(chunk_line, NULL, 16);
    } else {
        initial_chunk_size = 0x7FFFFFFF;
    }

    // Read WAV header
    int wav_header_size = 44;
    int wav_header_read = 0;
    uint8_t wav_header[64];

    while (wav_header_read < wav_header_size) {
        int r = recv(sockfd, wav_header + wav_header_read, wav_header_size - wav_header_read, 0);
        if (r <= 0) {
            LOG_E("Failed to read WAV header, recv=%d\r\n", r);
            break;
        }
        wav_header_read += r;
    }

    // Debug: print first bytes received
    LOG_I("WAV header bytes[0..7]: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
          wav_header[0], wav_header[1], wav_header[2], wav_header[3],
          wav_header[4], wav_header[5], wav_header[6], wav_header[7]);
    LOG_I("WAV header as text: %.8s\r\n", wav_header);

    // Verify WAV header
    if (wav_header[0] != 'R' || wav_header[1] != 'I' || wav_header[2] != 'F' || wav_header[3] != 'F') {
        LOG_E("Invalid WAV header (expected RIFF, got %c%c%c%c)\r\n",
              wav_header[0], wav_header[1], wav_header[2], wav_header[3]);
        goto cleanup;
    }

    // Parse WAV format info (standard WAV header layout)
    // Bytes 20-21: Audio format (1 = PCM)
    // Bytes 22-23: Number of channels
    // Bytes 24-27: Sample rate
    // Bytes 34-35: Bits per sample
    uint16_t audio_format = *(uint16_t*)(wav_header + 20);
    uint16_t num_channels = *(uint16_t*)(wav_header + 22);
    uint32_t sample_rate = *(uint32_t*)(wav_header + 24);
    uint16_t bits_per_sample = *(uint16_t*)(wav_header + 34);

    LOG_I("WAV Format: %d (1=PCM), Channels: %d, SampleRate: %d, Bits: %d\r\n",
          audio_format, num_channels, sample_rate, bits_per_sample);

    // Check if we need to handle sample rate mismatch
    bool need_resample = (sample_rate != 16000);
    bool is_stereo_input = (num_channels == 2);

    if (need_resample) {
        LOG_W("Sample rate mismatch! TTS=%d, I2S=16000\r\n", sample_rate);
    }

    // Find actual data offset
    int data_offset = 0;
    for (int i = 12; i < wav_header_read - 4; i++) {
        if (wav_header[i] == 'd' && wav_header[i+1] == 'a' &&
            wav_header[i+2] == 't' && wav_header[i+3] == 'a') {
            data_offset = i + 8;
            break;
        }
    }

    if (data_offset == 0) {
        LOG_E("Could not find data chunk\r\n");
        goto cleanup;
    }

    LOG_I("WAV data starts at offset %d\r\n", data_offset);

    // Set I2S sample rate to match TTS audio
    set_i2s_sample_rate(sample_rate);

    // Switch to playback mode
    switch_es8388_mode(ES8388_PLAY_BACK_MODE);
    
    // Mute DAC before starting playback to avoid pop noise
    extern int ES8388_Set_Voice_Volume(int volume);
    ES8388_Set_Voice_Volume(0);  // Mute
    
    // Clear TX FIFO to avoid any stale data
    bflb_i2s_feature_control(i2s0, I2S_CMD_CLEAR_TX_FIFO, 0);
    
    bflb_i2s_link_txdma(i2s0, true);
    
    // Give codec time to stabilize after mode switch
    vTaskDelay(pdMS_TO_TICKS(100));  // Increased from 50ms to 100ms
    
    // Unmute DAC with a slower, smoother ramp to avoid pop
    // Ramp from 0 to 50 in smaller steps (reduced from 60 to further minimize distortion)
    for (int vol = 0; vol <= 50; vol += 5) {
        ES8388_Set_Voice_Volume(vol);
        vTaskDelay(pdMS_TO_TICKS(10)); // Slower ramp (10ms per step)
    }
    ES8388_Set_Voice_Volume(50);  // Final volume (reduced to minimize clipping)

    int mono_pos = 0;

    // If there's data after WAV header in our buffer, process it
    if (wav_header_read > data_offset) {
        int extra = wav_header_read - data_offset;
        memcpy(mono_buffer, wav_header + data_offset, extra);
        mono_pos = extra;
    }

    LOG_I("Starting streaming playback...\r\n");

    // Simple double buffering without pre-fill
    int fill_buffer_idx = 0;  
    bool is_playing = false;

    // Chunked encoding state
    int chunk_remaining = initial_chunk_size;
    if (is_chunked) {
        chunk_remaining -= wav_header_read;
        LOG_I("Initial chunk remaining: %d bytes\r\n", chunk_remaining);
    }

    while (1) {
        // Receive data until we have a full chunk
        int space = TTS_CHUNK_SIZE - mono_pos;
        if (space > 0) {
            // If chunked and current chunk is empty, read next chunk size
            if (is_chunked && chunk_remaining == 0) {
                // Read CRLF after chunk data
                char temp[2];
                recv(sockfd, temp, 2, 0); 
                
                // Read next chunk size line
                char line[32];
                int line_pos = 0;
                while (line_pos < sizeof(line) - 1) {
                    int r = recv(sockfd, line + line_pos, 1, 0);
                    if (r <= 0) break;
                    if (line[line_pos] == '\n' && line_pos > 0 && line[line_pos-1] == '\r') {
                        line[line_pos-1] = '\0';
                        break;
                    }
                    line_pos++;
                }
                
                int next_size = strtol(line, NULL, 16);
                // LOG_I("Next chunk: %d bytes\r\n", next_size);
                
                if (next_size == 0) {
                    LOG_I("End of chunks\r\n");
                    break; // End of stream
                }
                chunk_remaining = next_size;
            }

            int recv_size = (space > TTS_RECV_BUF_SIZE) ? TTS_RECV_BUF_SIZE : space;
            if (is_chunked && recv_size > chunk_remaining) {
                recv_size = chunk_remaining;
            }

            int r = recv(sockfd, mono_buffer + mono_pos, recv_size, 0);

            if (r <= 0) {
                // End of stream (unexpected if chunked)
                LOG_I("Stream ended unexpectedly (received %d)\r\n", r);
                break;
            }
            
            if (is_chunked) {
                chunk_remaining -= r;
            }
            mono_pos += r;
        }

        // When we have a full chunk
        if (mono_pos >= TTS_CHUNK_SIZE) {
            int mono_samples = TTS_CHUNK_SIZE / 2;
            uint32_t stereo_len = mono_samples * 4;

            // Convert to stereo
            mono_to_stereo((int16_t *)mono_buffer, stereo_buffers[fill_buffer_idx], mono_samples);
            
            // If nothing is playing, start immediately
            if (!is_playing) {
                play_buffer_non_blocking(stereo_buffers[fill_buffer_idx], stereo_len);
                is_playing = true;
            } else {
                // Wait for previous buffer to complete with tight polling and timeout
                uint32_t wait_start = xTaskGetTickCount();
                while (!dma_transfer_done) {
                    if (xTaskGetTickCount() - wait_start > pdMS_TO_TICKS(1000)) {
                        LOG_W("DMA wait timeout in loop\r\n");
                        break;
                    }
                    taskYIELD();
                }
                // Start playing immediately
                play_buffer_non_blocking(stereo_buffers[fill_buffer_idx], stereo_len);
            }

            // Switch to other buffer
            fill_buffer_idx = (fill_buffer_idx + 1) % TTS_NUM_BUFFERS;
            mono_pos = 0;
        }
    }
    
    // Wait for final playback to finish
    if (is_playing) {
        uint32_t wait_start = xTaskGetTickCount();
        while (!dma_transfer_done) {
            if (xTaskGetTickCount() - wait_start > pdMS_TO_TICKS(1000)) {
                LOG_W("DMA wait timeout at end\r\n");
                break;
            }
            taskYIELD();
        }
    }

    // Play remaining partial data
    if (mono_pos > 0) {
        int mono_samples = mono_pos / 2;
        uint32_t stereo_len = mono_samples * 4;
        mono_to_stereo((int16_t *)mono_buffer, stereo_buffers[fill_buffer_idx], mono_samples);
        play_buffer_non_blocking(stereo_buffers[fill_buffer_idx], stereo_len);
        
        uint32_t wait_start = xTaskGetTickCount();
        while (!dma_transfer_done) {
            if (xTaskGetTickCount() - wait_start > pdMS_TO_TICKS(1000)) {
                LOG_W("DMA wait timeout for last chunk\r\n");
                break;
            }
            taskYIELD();
        }
    }

    LOG_I("Streaming TTS complete!\r\n");
    result = 0;

cleanup:
    // Mute DAC before stopping to avoid pop noise
    ES8388_Set_Voice_Volume(0);
    vTaskDelay(pdMS_TO_TICKS(20));
    


    // Close socket
    if (sockfd >= 0) {
        close(sockfd);
    }

    // Stop DMA and I2S
    if (dma0_ch1) {
        bflb_dma_channel_stop(dma0_ch1);
    }
    bflb_i2s_feature_control(i2s0, I2S_CMD_DATA_ENABLE, 0);

    // Restore I2S sample rate to recording rate (16kHz)
    set_i2s_sample_rate(RECORDING_SAMPLE_RATE);

    // Switch back to recording mode
    switch_es8388_mode(ES8388_RECORDING_MODE);
    bflb_i2s_link_rxdma(i2s0, true);

    // Free buffers (stereo_buffers are now static, no need to free)
    if (recv_buf) vPortFree(recv_buf);
    if (mono_buffer) vPortFree(mono_buffer);

    return result;
}

// Legacy functions for compatibility
int tts_synthesize(const char *text, uint8_t **audio_data, uint32_t *audio_len)
{
    LOG_W("tts_synthesize is deprecated, use tts_synthesize_and_play_streaming\r\n");
    *audio_data = NULL;
    *audio_len = 0;
    return -1;
}

int tts_play_audio(uint8_t *audio_data, uint32_t audio_len)
{
    LOG_W("tts_play_audio is deprecated, use tts_synthesize_and_play_streaming\r\n");
    return -1;
}
