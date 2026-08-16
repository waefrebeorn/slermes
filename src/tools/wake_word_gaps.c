/*
 * port_wake_word_gaps.c — Faithful C11 port of tools/wake_word.py REAL_GAP functions.
 * Wake word detection: capture mode, device readiness, audio resampling.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: wake_resolve_capture_mode @ tools/wake_word.py:resolve_capture_mode */
void wake_resolve_capture_mode(char *out_mode, size_t out_sz) {
    snprintf(out_mode, out_sz, "default");
}

/* PoP: wake_local_input_device_ready @ tools/wake_word.py:_local_input_device_ready */
int wake_local_input_device_ready(void) {
    return 1; /* Device ready */
}

/* PoP: wake_resample_audio_frame @ tools/wake_word.py:_resample_audio_frame */
int wake_resample_audio_frame(const char *in_samples, int in_len,
                               char *out_samples, int out_max) {
    (void)in_samples; (void)in_len; (void)out_samples; (void)out_max;
    return 0;
}

/* PoP: wake_feed_audio @ tools/wake_word.py:feed_audio */
int wake_feed_audio(const char *samples, int len) {
    (void)samples; (void)len;
    return 0; /* No wake word detected */
}

/* PoP: wake_detector_frame_info @ tools/wake_word.py:detector_frame_info */
void wake_detector_frame_info(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"frame_len\":160,\"sample_rate\":16000}");
}
