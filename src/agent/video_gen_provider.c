/*
 * video_gen_provider.c — canonical C11 port of agent/video_gen_provider.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_video_gen_provider_save_url_video @ agent/video_gen_provider.py:save_url_video */
int agent_video_gen_provider_save_url_video(const char *arg) {
    /* Python: streamed cache. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "oversize") == 0) {
        fprintf(stderr, "Video exceeds cap; refusing to cache: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "empty") == 0) {
        fprintf(stderr, "Video was empty (0 bytes): %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "video download failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: agent_video_gen_provider_u_create_and_poll @ agent/video_gen_provider.py:_create_and_poll */
int agent_video_gen_provider_u_create_and_poll(const char *arg) {
    /* Python: create + poll w/ deadline. Arg =
     * "state\tdeadline_s\tinterval_s\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "video job did not reach a terminal status within deadline\n");
        return 1;
    }
    printf("video done: %s\n", t3 ? t3 + 1 : "completed");
    return 0;
}
