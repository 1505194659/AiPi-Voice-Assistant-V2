#ifndef __VAD_H__
#define __VAD_H__

#include <stdint.h>
#include <stdbool.h>

// VAD Configuration
#define VAD_SAMPLE_RATE 16000
#define VAD_FRAME_SIZE_MS 30                    // 30ms frames
#define VAD_FRAME_SIZE (VAD_SAMPLE_RATE * VAD_FRAME_SIZE_MS / 1000)  // 480 samples
#define VAD_SILENCE_THRESHOLD 150               // Energy threshold for silence detection
#define VAD_SILENCE_DURATION_MS 800             // 800ms of silence = speech end
#define VAD_SILENCE_FRAMES (VAD_SILENCE_DURATION_MS / VAD_FRAME_SIZE_MS)
#define VAD_MIN_SPEECH_DURATION_MS 300          // Minimum speech duration to process
#define VAD_MIN_SPEECH_FRAMES (VAD_MIN_SPEECH_DURATION_MS / VAD_FRAME_SIZE_MS)

// VAD State
typedef struct {
    uint32_t silent_frames;        // Consecutive silent frames
    uint32_t speech_frames;        // Total speech frames detected
    bool speech_started;           // Has speech been detected?
    uint32_t energy_history[10];   // Energy history for adaptive thresholding
    uint32_t history_index;
} vad_state_t;

/**
 * @brief Initialize VAD state
 */
void vad_init(vad_state_t *state);

/**
 * @brief Process audio frame for VAD
 * @param state VAD state
 * @param samples Audio samples (mono, 16-bit)
 * @param num_samples Number of samples in frame
 * @return true if speech detected, false if silence
 */
bool vad_process_frame(vad_state_t *state, int16_t *samples, uint32_t num_samples);

/**
 * @brief Check if speech has ended (silence detected after speech)
 * @param state VAD state
 * @return true if speech has ended
 */
bool vad_speech_ended(vad_state_t *state);

/**
 * @brief Check if minimum speech duration met
 * @param state VAD state
 * @return true if enough speech detected
 */
bool vad_has_speech(vad_state_t *state);

#endif // __VAD_H__
