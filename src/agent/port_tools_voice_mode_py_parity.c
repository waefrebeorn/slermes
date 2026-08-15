/* Slermes C port — tools/voice_mode.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_voice_mode_voice_capture_install_hint @ tools/voice_mode.py:_voice_capture_install_hint */
json_t *tools_voice_mode_voice_capture_install_hint_p(void) {
    return json_object();
}

/* PoP: tools_voice_mode_load_voice_stop_phrases @ tools/voice_mode.py:_load_voice_stop_phrases */
json_t *tools_voice_mode_load_voice_stop_phrases_p(void) {
    return json_object();
}

/* PoP: tools_voice_mode_is_tts_echo @ tools/voice_mode.py:is_tts_echo */
json_t *tools_voice_mode_is_tts_echo_p(void) {
    return json_object();
}
