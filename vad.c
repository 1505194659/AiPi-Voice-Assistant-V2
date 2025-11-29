#include "vad.h"
#include <math.h>
#include <string.h>

void vad_init(vad_state_t *state) {
    memset(state, 0, sizeof(vad_state_t));
}

bool vad_process_frame(vad_state_t *state, int16_t *samples, uint32_t num_samples) {
    // Calculate frame energy (RMS)
    uint64_t sum_squares = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        int32_t sample = samples[i];
        sum_squares += (uint64_t)(sample * sample);
    }

    uint32_t energy = (uint32_t)sqrt((double)sum_squares / num_samples);

    // Store in history for adaptive thresholding
    state->energy_history[state->history_index] = energy;
    state->history_index = (state->history_index + 1) % 10;

    // Determine if this frame has speech
    bool is_speech = (energy > VAD_SILENCE_THRESHOLD);

    if (is_speech) {
        state->silent_frames = 0;
        state->speech_frames++;
        state->speech_started = true;
    } else {
        if (state->speech_started) {
            state->silent_frames++;
        }
    }

    return is_speech;
}

bool vad_speech_ended(vad_state_t *state) {
    // Speech has ended if we've detected speech before and now have enough silence
    return state->speech_started && (state->silent_frames >= VAD_SILENCE_FRAMES);
}

bool vad_has_speech(vad_state_t *state) {
    return state->speech_frames >= VAD_MIN_SPEECH_FRAMES;
}
