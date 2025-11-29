#ifndef TTS_CLIENT_H
#define TTS_CLIENT_H

#include <stdint.h>

/**
 * @brief Streaming TTS - synthesize and play audio in real-time
 *
 * This function connects to the TTS server, receives audio data in chunks,
 * and plays them using double-buffered DMA for minimal memory usage and latency.
 *
 * @param text Text to convert to speech
 * @return 0 on success, -1 on failure
 */
int tts_synthesize_and_play_streaming(const char *text);

/**
 * @brief [DEPRECATED] Send text to Fish Speech TTS server and get audio data
 * Use tts_synthesize_and_play_streaming() instead.
 */
int tts_synthesize(const char *text, uint8_t **audio_data, uint32_t *audio_len);

/**
 * @brief [DEPRECATED] Play audio through ES8388 speaker
 * Use tts_synthesize_and_play_streaming() instead.
 */
int tts_play_audio(uint8_t *audio_data, uint32_t audio_len);

#endif // TTS_CLIENT_H
