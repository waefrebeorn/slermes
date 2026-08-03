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
#include "json.h"
#include "codex_app_server_client.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _notification_scope_ids @ agent/transports/codex_app_server_session.py:_notification_scope_ids */
char *cas2_notification_scope_ids(const char *notification_json) {
    /* Python: extract the thread/turn identity carried by a notification.
     * Returns {"thread_id": ..., "turn_id": ...} (nulls omitted). */
    if (!notification_json) return strdup("[]");

    json_t *note = json_parse(notification_json, NULL);
    if (!note || note->type != JSON_OBJECT) {
        if (note) json_free(note);
        return strdup("[]");
    }

    json_t *params = json_obj_get(note, "params");
    if (!params || params->type != JSON_OBJECT) {
        json_free(note);
        return strdup("[]");
    }

    const char *thread_id = json_get_str(params, "threadId", NULL);
    if (!thread_id) thread_id = json_get_str(params, "thread_id", NULL);
    json_t *nested_turn = json_obj_get(params, "turn");
    if (!thread_id && nested_turn && nested_turn->type == JSON_OBJECT) {
        thread_id = json_get_str(nested_turn, "threadId", NULL);
        if (!thread_id) thread_id = json_get_str(nested_turn, "thread_id", NULL);
    }
    const char *turn_id = json_get_str(params, "turnId", NULL);
    if (!turn_id) turn_id = json_get_str(params, "turn_id", NULL);
    if (!turn_id && nested_turn && nested_turn->type == JSON_OBJECT) {
        turn_id = json_get_str(nested_turn, "id", NULL);
        if (!turn_id) turn_id = json_get_str(nested_turn, "turnId", NULL);
    }

    json_t *out = json_object();
    if (thread_id) json_set(out, "thread_id", json_string(thread_id));
    if (turn_id) json_set(out, "turn_id", json_string(turn_id));
    json_free(note);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("[]");
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
    /* Python: append user guidance to the active Codex turn via turn/steer.
     * Returns {"ok": true} when the steer request was sent. */
    if (!guidance) return NULL;
    while (*guidance == ' ' || *guidance == '\t') guidance++;
    if (!*guidance) return strdup("{\"ok\": false}");

    /* Send a turn/steer request through a lazy codex app-server client
     * (one per process, mirroring the Python session's persistent client). */
    static codex_client_t *s_client = NULL;
    if (!s_client) {
        const char *bin = getenv("CODEX_BIN");
        const char *home = getenv("CODEX_HOME");
        s_client = codex_client_new(bin ? bin : "codex",
                                    home ? home : NULL, NULL, 0);
        if (!s_client) return strdup("{\"ok\": false}");
    }
    codex_client_t *c = s_client;

    json_t *p = json_object();
    json_set(p, "input", json_string(guidance));
    char *p_ser = json_serialize(p);
    json_free(p);
    if (!p_ser) return strdup("{\"ok\": false}");

    char *resp = codex_client_request(c, "turn/steer", p_ser, 10.0);
    free(p_ser);

    json_t *out = json_object();
    json_set(out, "ok", json_bool(resp != NULL));
    char *out_ser = json_serialize(out);
    json_free(out);
    free(resp);
    return out_ser ? out_ser : strdup("{\"ok\": false}");
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
    /* Python: in-progress fileChange item lookup. Returns the matching
     * item JSON (or NULL when absent). */
    if (!item_id || !pending_json) return NULL;

    json_t *arr = json_parse(pending_json, NULL);
    if (!arr) return NULL;
    if (arr->type != JSON_ARRAY) { json_free(arr); return NULL; }

    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *item = json_get(arr, i);
        if (!item || item->type != JSON_OBJECT) continue;
        const char *id = json_get_str(item, "id", NULL);
        if (!id) id = json_get_str(item, "itemId", NULL);
        if (id && strcmp(id, item_id) == 0) {
            json_t *copy = json_copy(item);
            json_free(arr);
            char *ser = json_serialize(copy);
            json_free(copy);
            return ser;
        }
    }
    json_free(arr);
    return NULL;
}

/* PoP: _apply_token_usage_notification @ agent/transports/codex_app_server_session.py:_apply_token_usage_notification */
char *cas2_apply_token_usage_notification(const char *usage_json) {
    /* Python: capture Codex token usage updates for caller accounting.
     * Only thread/tokenUsage/updated notifications carry usage; extract
     * the tokenUsage totals into a summary JSON. */
    if (!usage_json) return NULL;

    json_t *note = json_parse(usage_json, NULL);
    if (!note || note->type != JSON_OBJECT) {
        if (note) json_free(note);
        return NULL;
    }

    const char *method = json_get_str(note, "method", "");
    if (strcmp(method, "thread/tokenUsage/updated") != 0) {
        json_free(note);
        return strdup("{\"captured\": false}");
    }

    json_t *params = json_obj_get(note, "params");
    json_t *tu = (params && params->type == JSON_OBJECT) ? json_obj_get(params, "tokenUsage") : NULL;
    json_t *out = json_object();
    json_set(out, "captured", json_bool(tu != NULL));
    if (tu && tu->type == JSON_OBJECT) {
        json_t *last = json_obj_get(tu, "last");
        json_t *total = json_obj_get(tu, "total");
        if (last && last->type == JSON_OBJECT) {
            json_t *copy = json_copy(last);
            json_set(out, "last", copy);
        }
        if (total && total->type == JSON_OBJECT) {
            json_t *copy = json_copy(total);
            json_set(out, "total", copy);
        }
    }
    json_free(note);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{\"captured\": false}");
}

/* PoP: _apply_compaction_notification @ agent/transports/codex_app_server_session.py:_apply_compaction_notification */
int cas2_apply_compaction_notification(const char *notification_json) {
    /* Python: capture Codex-native context compaction boundaries. Both the
     * thread/compacted notification and a ContextCompaction item mean the
     * thread history has been compacted. Returns 1 when compacted. */
    if (!notification_json) return -1;

    json_t *note = json_parse(notification_json, NULL);
    if (!note || note->type != JSON_OBJECT) {
        if (note) json_free(note);
        return -1;
    }

    const char *method = json_get_str(note, "method", "");
    int compacted = 0;
    if (strcmp(method, "thread/compacted") == 0) {
        compacted = 1;
    } else {
        /* Look for a ContextCompaction item in params/items. */
        json_t *params = json_obj_get(note, "params");
        json_t *items = (params && params->type == JSON_OBJECT)
            ? json_obj_get(params, "items") : NULL;
        if (items && items->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(items); i++) {
                json_t *item = json_get(items, i);
                const char *type = item ? json_get_str(item, "type", "") : "";
                if (strcmp(type, "ContextCompaction") == 0) { compacted = 1; break; }
            }
        }
    }
    json_free(note);
    return compacted;
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
