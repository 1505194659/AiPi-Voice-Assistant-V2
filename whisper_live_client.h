#ifndef __WHISPER_LIVE_CLIENT_H__
#define __WHISPER_LIVE_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>

// WhisperLive configuration
#define WHISPER_LIVE_MAX_HOST_LEN 64
#define WHISPER_LIVE_MAX_PATH_LEN 128
#define WHISPER_LIVE_RECV_BUF_SIZE 2048
#define WHISPER_LIVE_CHUNK_SAMPLES 4096  // WhisperLive expects 4096 samples per chunk

// WhisperLive client handle
typedef struct whisper_live_client_s {
    int socket_fd;
    char host[WHISPER_LIVE_MAX_HOST_LEN];
    int port;
    char path[WHISPER_LIVE_MAX_PATH_LEN];
    bool connected;
    bool config_sent;
    uint8_t recv_buffer[WHISPER_LIVE_RECV_BUF_SIZE];
} whisper_live_client_t;

/**
 * @brief Initialize WhisperLive client
 * @param client Client handle
 * @param server_url WebSocket server URL (e.g., "ws://192.168.1.151:9090/")
 * @return 0 on success, -1 on error
 */
int whisper_live_init(whisper_live_client_t *client, const char *server_url);

/**
 * @brief Connect to WhisperLive server
 * @param client Client handle
 * @return 0 on success, -1 on error
 */
int whisper_live_connect(whisper_live_client_t *client);

/**
 * @brief Send audio data to WhisperLive server
 * @param client Client handle
 * @param audio_data Audio data buffer (PCM 16-bit mono)
 * @param len Length of audio data in bytes
 * @return Number of bytes sent, -1 on error
 */
int whisper_live_send_audio(whisper_live_client_t *client, const uint8_t *audio_data, uint32_t len);

/**
 * @brief Receive transcription from WhisperLive server
 * @param client Client handle
 * @param buffer Buffer to store transcription
 * @param buffer_size Size of buffer
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes received, 0 on timeout, -1 on error
 */
int whisper_live_recv_transcription(whisper_live_client_t *client, char *buffer, uint32_t buffer_size, uint32_t timeout_ms);

/**
 * @brief Send END_OF_AUDIO signal to server
 * @param client Client handle
 * @return 0 on success, -1 on error
 */
int whisper_live_send_end_of_audio(whisper_live_client_t *client);

/**
 * @brief Disconnect from WhisperLive server
 * @param client Client handle
 */
void whisper_live_disconnect(whisper_live_client_t *client);

#endif // __WHISPER_LIVE_CLIENT_H__
