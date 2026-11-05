/*
 * t_port_input_sanitize.c — Oracle harness for
 * hermes_cli/input_sanitize.py (ported to src/cli/port_input_sanitize.c).
 *
 * Emits one JSON line per case: the input plus the results of all three
 * ported helpers. The Python oracle replays the LIVE module over the same
 * inputs and compares byte-for-byte.
 */
#include "port_input_sanitize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUT_CAP 65536

static void json_escape(const char *s, char *dst, size_t cap)
{
    size_t o = 0;
    if (!s) { if (cap) dst[0] = '\0'; return; }
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 8 < cap; p++) {
        switch (*p) {
        case '"':  dst[o++] = '\\'; dst[o++] = '"';  break;
        case '\\': dst[o++] = '\\'; dst[o++] = '\\'; break;
        case '\n': dst[o++] = '\\'; dst[o++] = 'n';  break;
        case '\r': dst[o++] = '\\'; dst[o++] = 'r';  break;
        case '\t': dst[o++] = '\\'; dst[o++] = 't';  break;
        case '\f': dst[o++] = '\\'; dst[o++] = 'f';  break;
        case '\v': dst[o++] = '\\'; dst[o++] = 'v';  break;
        default:
            if (*p < 0x20) {
                o += (size_t)snprintf(dst + o, cap - o, "\\u%04x", *p);
            } else {
                dst[o++] = (char)*p;
            }
        }
    }
    dst[o] = '\0';
}

static void emit(const char *name, const char *input)
{
    char ein[OUT_CAP], estrip[OUT_CAP], ecoll[OUT_CAP], esan[OUT_CAP];
    char *strip, *coll, *san;

    strip = input_sanitize_strip_leaked_bracketed_paste_wrappers(input);
    coll  = input_sanitize_collapse_repeated_input_artifacts(input, 4);
    san   = input_sanitize_sanitize_user_prompt_text(input);

    json_escape(input, ein, sizeof(ein));
    json_escape(strip, estrip, sizeof(estrip));
    json_escape(coll, ecoll, sizeof(ecoll));
    json_escape(san, esan, sizeof(esan));

    printf("{\"case\":\"%s\",\"in\":%s%s%s,\"strip\":%s%s%s,\"collapse\":%s%s%s,"
           "\"sanitize\":%s%s%s}\n",
           name,
           input ? "\"" : "", input ? ein : "null", input ? "\"" : "",
           strip ? "\"" : "", strip ? estrip : "null", strip ? "\"" : "",
           coll ? "\"" : "", coll ? ecoll : "null", coll ? "\"" : "",
           san ? "\"" : "", san ? esan : "null", san ? "\"" : "");

    free(strip);
    free(coll);
    free(san);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    emit("null_input", NULL);
    emit("empty", "");
    emit("plain", "hello world");
    emit("canonical_esc_200", "\x1b[200~pasted text");
    emit("canonical_esc_201", "pasted text\x1b[201~");
    emit("canonical_caret_200", "^[[200~pasted");
    emit("canonical_caret_201", "pasted^[[201~");
    emit("degraded_200_at_start", "[200~prompt");
    emit("degraded_200_after_space", "say [200~prompt");
    emit("degraded_200_embedded", "literal[200~tag");
    emit("degraded_201_at_end", "prompt[201~");
    emit("degraded_201_before_punct", "prompt[201~.");
    emit("degraded_201_midword", "prompt[201~word");
    emit("degraded_00_at_start", "00~prompt");
    emit("degraded_00_after_colon", "note:00~body");
    emit("degraded_00_embedded", "a00~b");
    emit("degraded_01_at_end", "prompt01~");
    emit("degraded_01_before_bracket", "prompt01~[x]");
    emit("degraded_01_midword", "prompt01~word");
    /* trailing artifact runs: marker is "~[[e" (4 bytes) */
    emit("artifact_x3", "text~[[e~[[e~[[e");
    emit("artifact_x4", "text~[[e~[[e~[[e~[[e");
    emit("artifact_x4_prefixed_e", "text[e~[[e~[[e~[[e~[[e");
    emit("artifact_x4_prefixed_bracket", "text[~[[e~[[e~[[e~[[e");
    emit("artifact_x5_prefixed_e", "text[e~[[e~[[e~[[e~[[e~[[e");
    /* sanitize composes both stages */
    emit("combo", "[200~hi~[[e~[[e~[[e~[[e[201~.");
    emit("utf8_payload", "\xc3\xa9t\xc3\xa9 [200~ok");
    return 0;
}
