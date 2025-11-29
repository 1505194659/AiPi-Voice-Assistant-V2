#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "cJSON.h"
#include "stt_client.h"
#include "whisper_live_client.h"

#define DBG_TAG "STT"

// Global WhisperLive client instance
whisper_live_client_t g_whisper_client;

// Initialize STT service
int stt_init(const char *server_url) {
    LOG_I("Initializing STT service with WhisperLive: %s\r\n", server_url);

    if (whisper_live_init(&g_whisper_client, server_url) < 0) {
        LOG_E("Failed to initialize WhisperLive client\r\n");
        return -1;
    }

    LOG_I("STT service initialized\r\n");
    return 0;
}

// Connect to STT service
int stt_connect(void) {
    LOG_I("Connecting to STT service...\r\n");

    if (whisper_live_connect(&g_whisper_client) < 0) {
        LOG_E("Failed to connect to WhisperLive server\r\n");
        return -1;
    }

    LOG_I("Connected to STT service\r\n");
    return 0;
}

// Send audio chunk to STT server (real-time streaming)
// Input: stereo int16 PCM data (L, R, L, R, ...)
// Output: mono float32 normalized to [-1, 1]
// Static buffer to avoid malloc - supports up to 1000ms of audio at 16kHz
#define MAX_FLOAT_SAMPLES (16000 * 1)  // 16000 samples = 1000ms mono
static float static_float_buffer[MAX_FLOAT_SAMPLES];

int stt_send_audio_chunk(uint8_t *audio_data, uint32_t len) {
    if (!audio_data || len == 0) {
        return -1;
    }

    // Input: stereo int16 = len bytes = len/2 samples = len/4 frames
    int16_t *samples = (int16_t*)audio_data;
    uint32_t num_stereo_samples = len / 2;  // Total int16 samples
    uint32_t num_frames = num_stereo_samples / 2;  // Number of L+R pairs

    // Check buffer size
    if (num_frames > MAX_FLOAT_SAMPLES) {
        LOG_E("Audio chunk too large: %d frames (max %d)\r\n", num_frames, MAX_FLOAT_SAMPLES);
        return -1;
    }

    // Convert stereo int16 to mono float32, using left channel only
    for (uint32_t i = 0; i < num_frames; i++) {
        static_float_buffer[i] = (float)samples[i * 2] / 32768.0f;  // Left channel only
    }

    // Send float32 audio data to WhisperLive
    uint32_t float_len = num_frames * sizeof(float);
    int sent = whisper_live_send_audio(&g_whisper_client, (uint8_t*)static_float_buffer, float_len);

    if (sent < 0) {
        LOG_E("Failed to send audio chunk\r\n");
        return -1;
    }

    LOG_D("Sent %d bytes of audio (%d samples)\r\n", sent, num_frames);
    return sent;
}

// Receive transcription from STT server
int stt_recv_transcription(char *buffer, uint32_t buffer_size, uint32_t timeout_ms) {
    if (!buffer) {
        return -1;
    }

    // Receive raw WebSocket message
    char raw_buffer[1024];
    int received = whisper_live_recv_transcription(&g_whisper_client, raw_buffer, sizeof(raw_buffer), timeout_ms);

    if (received > 0) {
        LOG_I("Received raw message: %s\r\n", raw_buffer);

        // Try to parse as JSON
        cJSON *json = cJSON_Parse(raw_buffer);
        if (json) {
            // WhisperLive returns "segments" array with transcription results
            cJSON *segments = cJSON_GetObjectItem(json, "segments");
            if (segments && cJSON_IsArray(segments)) {
                // Concatenate all segment texts
                buffer[0] = '\0';
                int total_len = 0;
                cJSON *segment;
                cJSON_ArrayForEach(segment, segments) {
                    cJSON *text_item = cJSON_GetObjectItem(segment, "text");
                    if (text_item && cJSON_IsString(text_item)) {
                        int text_len = strlen(text_item->valuestring);
                        if (total_len + text_len < buffer_size - 1) {
                            strcat(buffer, text_item->valuestring);
                            total_len += text_len;
                        }
                    }
                }
                if (total_len > 0) {
                    cJSON_Delete(json);
                    LOG_I("Parsed segments transcription: %s\r\n", buffer);
                    return total_len;
                }
            }

            // Look for "text" field (alternative format)
            cJSON *text_item = cJSON_GetObjectItem(json, "text");
            if (text_item && cJSON_IsString(text_item)) {
                strncpy(buffer, text_item->valuestring, buffer_size - 1);
                buffer[buffer_size - 1] = '\0';
                cJSON_Delete(json);
                LOG_I("Parsed transcription: %s\r\n", buffer);
                return strlen(buffer);
            }

            // Look for "message" field (server status messages) - ignore these
            cJSON *msg_item = cJSON_GetObjectItem(json, "message");
            if (msg_item && cJSON_IsString(msg_item)) {
                LOG_I("Server status (ignoring): %s\r\n", msg_item->valuestring);
                cJSON_Delete(json);
                return 0;  // Return 0 to indicate no transcription, not an error
            }

            cJSON_Delete(json);
            LOG_W("JSON does not contain recognized fields\r\n");
            // Return raw message if no recognized field
            strncpy(buffer, raw_buffer, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return strlen(buffer);
        } else {
            // Not JSON, treat as plain text
            LOG_I("Not JSON, treating as plain text\r\n");
            strncpy(buffer, raw_buffer, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return strlen(buffer);
        }
    } else if (received == 0) {
        LOG_I("Transcription timeout\r\n");
    } else {
        LOG_E("Failed to receive transcription\r\n");
    }

    return received;
}

// Send END_OF_AUDIO signal
int stt_send_end_of_audio(void) {
    LOG_I("Sending END_OF_AUDIO signal\r\n");
    return whisper_live_send_end_of_audio(&g_whisper_client);
}

// Disconnect from STT service
void stt_disconnect(void) {
    LOG_I("Disconnecting from STT service...\r\n");
    whisper_live_disconnect(&g_whisper_client);
}

// Check if STT service is connected
bool stt_is_connected(void) {
    return g_whisper_client.connected;
}
