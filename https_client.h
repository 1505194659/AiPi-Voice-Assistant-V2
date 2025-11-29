#ifndef HTTPS_CLIENT_H
#define HTTPS_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform an HTTPS request (for normal API responses, max 16KB)
 *
 * @param url Full URL (e.g., "https://api.openai.com/v1/chat/completions")
 * @param method HTTP Method (GET, POST)
 * @param headers Additional headers (must end with \r\n)
 * @param body Request body (NULL if none)
 * @param body_len Length of request body
 * @return char* Response body (must be freed by caller) or NULL on failure
 */
char* https_request(const char* url, const char* method, const char* headers,
                   const char* body, int body_len);

/**
 * @brief Perform an HTTPS request for large responses (for TTS, max 256KB)
 *
 * @param url Full URL
 * @param method HTTP Method (GET, POST)
 * @param headers Additional headers (must end with \r\n)
 * @param body Request body (NULL if none)
 * @param body_len Length of request body
 * @return char* Response body (must be freed by caller) or NULL on failure
 */
char* https_request_large(const char* url, const char* method, const char* headers,
                          const char* body, int body_len);

/**
 * @brief Specialized function to stream audio data without allocating a huge buffer
 */
char* https_post_audio(const char* url, const char* headers,
                      const char* part1, int part1_len,
                      const char* part2, int part2_len,
                      const char* part3, int part3_len,
                      const char* part4, int part4_len);

#ifdef __cplusplus
}
#endif

#endif // HTTPS_CLIENT_H
