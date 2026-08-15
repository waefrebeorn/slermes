/* Slermes C port — tools/transcription_tools.py (remaining gaps) */
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

/* PoP: tools_transcription_tools_get_local_command_template @ tools/transcription_tools.py:_get_local_command_template */
json_t *tools_transcription_tools_get_local_command_template_p(void) {
    return json_object();
}

/* PoP: tools_transcription_tools_command_stt_env_passthrough @ tools/transcription_tools.py:_command_stt_env_passthrough */
json_t *tools_transcription_tools_command_stt_env_passthrough_p(void) {
    return json_object();
}
