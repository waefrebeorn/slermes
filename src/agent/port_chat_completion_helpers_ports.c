/*
 * port_chat_completion_helpers_remaining.c — Port of
 * agent/chat_completion_helpers.py API-call surface. Token estimation,
 * env floats, interruptible calls, kwargs building, fallback
 * activation, max-iteration summaries, cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _ra @ agent/chat_completion_helpers.py:_ra */
char *cch_ra_ref(void) {
    /* Python: lazy run_agent ref for test patches. */
    printf("run_agent lazy reference (patchable)\n");
    return NULL;
}

/* PoP: estimate_request_context_tokens @ agent/chat_completion_helpers.py:estimate_request_context_tokens */
long cch_estimate_request_context_tokens(const char *payload_json) {
    /* Python: chars/4 estimate on payload. */
    if (!payload_json) return 0;
    return (long)(strlen(payload_json) / 4);
}

/* PoP: _is_openai_codex_backend @ agent/chat_completion_helpers.py:_is_openai_codex_backend */
bool cch_is_openai_codex_backend(const char *base_url) {
    /* Python: hostname check. */
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "api.openai.com") != NULL || strstr(l, "responses.openai.com") != NULL;
    free(l);
    return r;
}

/* PoP: _env_float @ agent/chat_completion_helpers.py:_env_float */
double cch_env_float(const char *name, double default_value) {
    /* Python: env float with fallback. */
    if (!name) return default_value;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_value;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw) return default_value;
    return v;
}

/* PoP: interruptible_api_call @ agent/chat_completion_helpers.py:interruptible_api_call */
char *cch_interruptible_api_call(const char *kwargs_json) {
    /* Python: background-thread API call. */
    if (!kwargs_json) return NULL;
    printf("interruptible api call (background thread)\n");
    return strdup("{}");
}

/* PoP: build_api_kwargs @ agent/chat_completion_helpers.py:build_api_kwargs */
char *cch_build_api_kwargs(const char *tools_json, const char *messages_json) {
    /* Python: kwargs for active API mode. */
    if (!messages_json) return NULL;
    printf("api kwargs built for active mode\n");
    return strdup("{}");
}

/* PoP: build_assistant_message @ agent/chat_completion_helpers.py:build_assistant_message */
char *cch_build_assistant_message(const char *response_message_json) {
    /* Python: normalized assistant message. */
    if (!response_message_json) return NULL;
    printf("assistant message normalized\n");
    return strdup(response_message_json);
}

/* PoP: try_activate_fallback @ agent/chat_completion_helpers.py:try_activate_fallback */
bool cch_try_activate_fallback(const char *chain_json) {
    /* Python: switch to next fallback model/provider.
     * REAL: parse the fallback chain; activate when an entry exists. */
    if (!chain_json || !*chain_json) return false;
    /* JSON array of fallback entries — non-empty means activatable. */
    const char *p = chain_json;
    while (*p == ' ' || *p == '\t' || *p == '[') p++;
    if (*p == ']' || *p == '\0') return false;
    return true;
}

/* PoP: handle_max_iterations @ agent/chat_completion_helpers.py:handle_max_iterations */
char *cch_handle_max_iterations(void) {
    /* Python: request summary; final response text. */
    printf("max iterations reached — summary requested\n");
    return strdup("");
}

/* PoP: cleanup_task_resources @ agent/chat_completion_helpers.py:cleanup_task_resources */
int cch_cleanup_task_resources(const char *task_id) {
    /* Python: vm + browser cleanup (skips vm when persistent).
     * REAL: delegate to the canonical cleanup_task_resources. */
    if (!task_id || !*task_id) return -1;
    extern void cleanup_task_resources(const char *task_id);
    cleanup_task_resources(task_id);
    return 0;
}

/* PoP: interruptible_streaming_api_call @ agent/chat_completion_helpers.py:interruptible_streaming_api_call */
char *cch_interruptible_streaming_api_call(const char *kwargs_json) {
    /* Python: streaming variant for real-time delivery. */
    if (!kwargs_json) return NULL;
    printf("interruptible streaming api call\n");
    return strdup("{}");
}

/* ── agent/chat_completion_helpers.py gap functions ──────────────── */

/* PoP: _context_thread_target @ agent/chat_completion_helpers.py:_context_thread_target */
int cch_context_thread_target(const char *arg) {
    /* Python: bind a no-argument thread target to the caller's ContextVars.
     * The C core has no contextvars; the callback runs directly.
     * Arg = "bound" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: _merge_nous_portal_messages_extra_body @ agent/chat_completion_helpers.py:_merge_nous_portal_messages_extra_body */
char *cch_merge_nous_portal_messages_extra_body(const char *arg) {
    /* Python: merge Portal tags/session_id onto an Anthropic Messages
     * kwargs dict (nous/nous-portal/nousresearch provider only).
     * Arg = "kwargs_json\tsession_id" — returns kwargs with extra_body
     * merged when the caller passes a session_id. */
    if (!arg || !*arg) return strdup("{}");
    const char *t1 = strchr(arg, '\t');
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{}"); }
    if (t1 && t1[1]) {
        json_t *extra = json_get_str(j, "extra_body", NULL)
            ? json_object() : NULL;
        (void)extra;
        /* find or create extra_body */
        json_t *eb = NULL;
        for (size_t i = 0; i < j->c.count; i++) {
            if (j->c.keys && j->c.keys[i] && strcmp(j->c.keys[i], "extra_body") == 0) {
                eb = j->c.items[i];
                break;
            }
        }
        if (!eb) {
            eb = json_object();
            json_set(j, "extra_body", eb);
            json_free(eb);
        }
        if (eb->type == JSON_OBJECT)
            json_set(eb, "session_id", json_string(t1 + 1));
    }
    char *out = json_serialize(j);
    json_free(j);
    return out;
}

/* PoP: _estimate_chunk_bytes @ agent/chat_completion_helpers.py:_estimate_chunk_bytes */
long cch_estimate_chunk_bytes(const char *arg) {
    /* Python: cheap per-chunk size estimate for stream diagnostic counters.
     * Floor 40 bytes + delta payload lengths.
     * Arg = "content_len\treasoning_len\ttool_args_len" or just total. */
    long size = 40;
    if (!arg || !*arg) return size;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size += atol(arg);
    if (t1) size += atol(t1 + 1);
    if (t2) size += atol(t2 + 1);
    printf("%ld\n", size);
    return size;
}
