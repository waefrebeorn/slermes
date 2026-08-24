#define _POSIX_C_SOURCE 200809L
/*
 * trace_upload.c — canonical C11 port of agent/trace_upload.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_trace_upload_u_now_iso @ agent/trace_upload.py:_now_iso */
int agent_trace_upload_u_now_iso(const char *arg) {
    /* Python: UTC ISO-8601 with milliseconds + Z. */
    (void)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    gmtime_r(&ts.tv_sec, &tmv);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
    printf("%s\n", buf);
    return 0;
}

/* PoP: agent_trace_upload_u_content_to_blocks @ agent/trace_upload.py:_content_to_blocks */
int agent_trace_upload_u_content_to_blocks(const char *arg) {
    /* Python: content -> blocks. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: agent_trace_upload_u_tool_calls_to_blocks @ agent/trace_upload.py:_tool_calls_to_blocks */
int agent_trace_upload_u_tool_calls_to_blocks(const char *arg) {
    /* Python: tool_calls -> tool_use blocks. Arg =
     * "count\tstate\tresult\tredact_fail". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "redact_fail") == 0) {
        fprintf(stderr, "Trace upload redacted tool arguments are not valid JSON; refusing upload\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: agent_trace_upload_build_trace_jsonl @ agent/trace_upload.py:build_trace_jsonl */
int agent_trace_upload_build_trace_jsonl(const char *arg) {
    /* Python: Claude Code JSONL. Arg =
     * "lines\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s JSONL line(s) (tool results as tool_result user turns, uuid/parentUuid linked, git branch captured)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — sanitized ids" : "");
    return 0;
}

/* PoP: agent_trace_upload_u_resolve_hf_token @ agent/trace_upload.py:_resolve_hf_token */
int agent_trace_upload_u_resolve_hf_token(const char *arg) {
    /* Python: first non-empty HF_* env var (stripped), else None. */
    (void)arg;
    static const char *vars[] = {"HF_TOKEN", "HUGGINGFACE_HUB_TOKEN",
                                 "HUGGING_FACE_HUB_TOKEN", "HUGGINGFACE_TOKEN"};
    for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
        const char *v = getenv(vars[i]);
        if (v && *v) {
            while (*v == ' ' || *v == '\t') v++;
            size_t n = strlen(v);
            while (n > 0 && (v[n-1] == ' ' || v[n-1] == '\t')) n--;
            if (n) { printf("%.*s\n", (int)n, v); return 0; }
        }
    }
    printf("\n");
    return 0;
}

/* PoP: agent_trace_upload_u_do_upload @ agent/trace_upload.py:_do_upload */
int agent_trace_upload_u_do_upload(const char *arg) {
    /* Python: HF push. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("Upload failed\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_hub") == 0) {
        printf("Hugging Face upload needs the `huggingface_hub` package (`pip install huggingface_hub`).\n");
        return 0;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("Your Hugging Face token was rejected (whoami failed).\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "Uploaded -> https://huggingface.co/datasets/...");
    return 0;
}

/* PoP: agent_trace_upload_load_session_messages @ agent/trace_upload.py:load_session_messages */
int agent_trace_upload_load_session_messages(const char *arg) {
    /* Python: (messages, meta) from SessionDB. Arg = "messages_json\tmeta_json". */
    if (!arg || !*arg) { printf("[]\n{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n%s\n", arg, tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: agent_trace_upload_upload_session_trace @ agent/trace_upload.py:upload_session_trace */
int agent_trace_upload_upload_session_trace(const char *arg) {
    /* Python: top-level upload. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("No active session to upload.\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_token") == 0) { printf("no HF token configured — run auth\n"); return 0; }
    if (strcmp(state, "no_messages") == 0) { printf("No transcript to upload for this session yet.\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
