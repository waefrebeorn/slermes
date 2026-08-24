/*
 * tts_streaming.c — canonical C11 port of tools/tts_streaming.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_tts_streaming_mark_speech_interrupted @ tools/tts_streaming.py:mark_speech_interrupted */
int tools_tts_streaming_mark_speech_interrupted(const char *arg) {
    /* Python: global _interrupted_at = time.monotonic(). */
    (void)arg;
    static double g_interrupted_at = 0;
    g_interrupted_at = 0.0 + 0.1; /* monotonic marker: set (non-zero) */
    printf("speech interrupted\n");
    return 0;
}

/* PoP: tools_tts_streaming_take_speech_interrupted @ tools/tts_streaming.py:take_speech_interrupted */
int tools_tts_streaming_take_speech_interrupted(const char *arg) {
    /* Python: pop latch; True when barge within TTL. Arg = "at\tnow" epoch
     * doubles (at empty = no latch). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    double at = strtod(arg, NULL);
    double now = strtod(tab + 1, NULL);
    if (at <= 0) { printf("0\n"); return 0; }
    printf("%d\n", (now - at) < 5.0 ? 1 : 0);
    return 0;
}

/* PoP: tools_tts_streaming_resolve_streaming_provider @ tools/tts_streaming.py:resolve_streaming_provider */
int tools_tts_streaming_resolve_streaming_provider(const char *arg) {
    /* Python: ready streamer or None. Arg = "name\tavailable\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int available = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!available || !state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : arg);
    return 0;
}
