#ifndef STT_CLIENT_H
#define STT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "whisper_live_client.h"

// Global WhisperLive client
extern whisper_live_client_t g_whisper_client;

/**
 * @brief Initialize STT service (WhisperLive)
 * @param server_url WebSocket server URL (e.g., "ws://192.168.1.151:9090/")
 * @return 0 on success, -1 on failure
 */
int stt_init(const char *server_url);

/**
 * @brief Connect to STT service
 * @return 0 on success, -1 on failure
 */
int stt_connect(void);

/**
 * @brief Send audio data chunk to STT server (real-time streaming)
 * @param audio_data Pointer to PCM audio data (stereo 16-bit)
 * @param len Length of audio data in bytes
 * @return Number of bytes sent or -1 on failure
 */
int stt_send_audio_chunk(uint8_t *audio_data, uint32_t len);

/**
 * @brief Receive transcription from STT server (non-blocking with timeout)
 * @param buffer Buffer to store transcription text
 * @param buffer_size Size of buffer
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes received, 0 on timeout, -1 on error
 */
int stt_recv_transcription(char *buffer, uint32_t buffer_size, uint32_t timeout_ms);

/**
 * @brief Send END_OF_AUDIO signal to server to trigger final transcription
 * @return 0 on success, -1 on failure
 */
int stt_send_end_of_audio(void);

/**
 * @brief Disconnect from STT service
 */
void stt_disconnect(void);

/**
 * @brief Check if STT service is connected
 * @return true if connected
 */
bool stt_is_connected(void);

#endif // STT_CLIENT_H
