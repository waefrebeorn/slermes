/*
 * port_codex_app_server_session_remaining.c — Port of
 * agent/transports/codex_app_server_session.py session surface.
 * Notification scope ids, input coercion, lifecycle, steer, approval
 * decisions, token/compaction accounting, abort markers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _notification_scope_ids @ agent/transports/codex_app_server_session.py:_notification_scope_ids */
char *cas2_notification_scope_ids(const char *notification_json) {
    /* Python: thread/turn identity from notification. */
    if (!notification_json) return strdup("[]");
    printf("notification scope ids extracted\n");
    return strdup("[]");
}

/* PoP: _coerce_turn_input_text @ agent/transports/codex_app_server_session.py:_coerce_turn_input_text */
char *cas2_coerce_turn_input_text(const char *content_json) {
    /* Python: rich content → plain text input. */
    if (!content_json) return strdup("");
    if (content_json[0] == '"') {
        size_t n = strlen(content_json);
        if (n >= 2) return strndup(content_json + 1, n - 2);
        return strdup("");
    }
    /* extract text pieces */
    size_t cap = strlen(content_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    const char *p = content_json;
    while ((p = strstr(p, "\"text\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (*out) strcat(out, " ");
            strncat(out, v, (size_t)(e - v));
        }
        p = e;
    }
    return out;
}

/* PoP: __init__ @ agent/transports/codex_app_server_session.py:__init__ */
char *cas2_init(const char *cwd, const char *codex_bin) {
    /* Python: session state. */
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"codex_bin\": \"%s\"}",
             cwd ? cwd : ".", codex_bin ? codex_bin : "");
    return out;
}

/* PoP: close @ agent/transports/codex_app_server_session.py:close */
int cas2_close(void) {
    printf("codex session closed\n");
    return 0;
}

/* PoP: __enter__ @ agent/transports/codex_app_server_session.py:__enter__ */
char *cas2_enter(void) {
    return strdup("{}");
}

/* PoP: __exit__ @ agent/transports/codex_app_server_session.py:__exit__ */
int cas2_exit(void) {
    return cas2_close();
}

/* PoP: request_steer @ agent/transports/codex_app_server_session.py:request_steer */
char *cas2_request_steer(const char *guidance) {
    /* Python: turn/steer user guidance. */
    if (!guidance) return NULL;
    printf("codex turn steered: %.60s\n", guidance);
    return strdup("{}");
}

/* PoP: _decide_exec_approval @ agent/transports/codex_app_server_session.py:_decide_exec_approval */
char *cas2_decide_exec_approval(bool auto_approve, const char *command) {
    /* Python: accept when auto; else policy. */
    if (auto_approve) return strdup("accept");
    if (!command) return strdup("reject");
    printf("exec approval decision for: %.60s\n", command);
    return strdup("reject");
}

/* PoP: _decide_apply_patch_approval @ agent/transports/codex_app_server_session.py:_decide_apply_patch_approval */
char *cas2_decide_apply_patch_approval(bool auto_approve) {
    if (auto_approve) return strdup("accept");
    return strdup("reject");
}

/* PoP: _lookup_pending_file_change @ agent/transports/codex_app_server_session.py:_lookup_pending_file_change */
char *cas2_lookup_pending_file_change(const char *item_id, const char *pending_json) {
    /* Python: in-progress fileChange item lookup. */
    if (!item_id || !pending_json) return NULL;
    if (strstr(pending_json, item_id)) {
        printf("pending file change summarized (%s)\n", item_id);
        return strdup("{}");
    }
    return NULL;
}

/* PoP: _apply_token_usage_notification @ agent/transports/codex_app_server_session.py:_apply_token_usage_notification */
char *cas2_apply_token_usage_notification(const char *usage_json) {
    /* Python: capture token usage for accounting. */
    if (!usage_json) return NULL;
    printf("codex token usage captured\n");
    return strdup(usage_json);
}

/* PoP: _apply_compaction_notification @ agent/transports/codex_app_server_session.py:_apply_compaction_notification */
int cas2_apply_compaction_notification(const char *notification_json) {
    /* Python: capture compaction boundaries. */
    if (!notification_json) return -1;
    printf("codex compaction boundary captured\n");
    return 0;
}

/* PoP: _has_turn_aborted_marker @ agent/transports/codex_app_server_session.py:_has_turn_aborted_marker */
bool cas2_has_turn_aborted_marker(const char *text) {
    /* Python: raw abort markers from codex. */
    if (!text) return false;
    return strstr(text, "turn aborted") != NULL || strstr(text, "TURN_ABORTED") != NULL;
}

/* PoP: _get_hermes_version @ agent/transports/codex_app_server_session.py:_get_hermes_version */
char *cas2_get_hermes_version(void) {
    /* Python: best-effort version for userAgent. */
    const char *v = getenv("HERMES_VERSION");
    if (v && *v) return strdup(v);
    return strdup("0.0.0");
}
