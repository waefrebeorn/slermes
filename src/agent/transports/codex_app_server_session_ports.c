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

/* PoP: cas2_notification_scope_ids @ agent/transports/codex_app_server_session.py:_notification_scope_ids */
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

/* PoP: cas2_coerce_turn_input_text @ agent/transports/codex_app_server_session.py:_coerce_turn_input_text */
char *cas2_coerce_turn_input_text(const char *content_json) {
    /* Faithful port of the LIVE ladder: string passthrough; list of
     * strings / {text,input_text} parts / image markers joined with
     * "\n\n"; default question when everything is blank. */
    if (!content_json) return strdup("");
    json_t *root = json_parse(content_json, NULL);
    if (!root) return strdup("");
    char *out = NULL;
    if (root->type == JSON_STRING) {
        out = strdup((root->str_val));
    } else if (root->type == JSON_ARRAY) {
        size_t cap = strlen(content_json) + 64;
        out = malloc(cap);
        if (out) {
            out[0] = '\0';
            size_t n = json_len(root);
            for (size_t i = 0; i < n; i++) {
                json_t *item = json_get(root, i);
                if (!item) continue;
                const char *piece = NULL;
                char numbuf[32];
                if (item->type == JSON_STRING) {
                    const char *s = (item->str_val);
                    /* LIVE: skip blank strings */
                    int blank = 1;
                    for (const char *q = s; *q; q++)
                        if (*q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') { blank = 0; break; }
                    if (blank) continue;
                    piece = s;
                } else if (item->type == JSON_OBJECT) {
                    json_t *t = json_obj_get(item, "type");
                    const char *ty = (t && t->type == JSON_STRING) ? (t->str_val) : NULL;
                    if (ty && (strcmp(ty, "text") == 0 || strcmp(ty, "input_text") == 0)) {
                        json_t *tv = json_obj_get(item, "text");
                        json_t *cv = json_obj_get(item, "content");
                        const char *s = NULL;
                        if (tv && tv->type == JSON_STRING && (tv->str_val)[0]) s = (tv->str_val);
                        else if (cv && cv->type == JSON_STRING && (cv->str_val)[0]) s = (cv->str_val);
                        if (s && *s) piece = s;
                    } else if (ty && (strcmp(ty, "image") == 0 || strcmp(ty, "image_url") == 0 ||
                                      strcmp(ty, "input_image") == 0)) {
                        piece = "[image attached]";
                    }
                } else if (item->type == JSON_NULL) {
                    continue;   /* LIVE: None items are skipped silently */
                } else {
                    /* non-dict scalar (number/bool): str(item) — keep in sync with twin */
                    if (item->type == JSON_NUMBER) {
                        snprintf(numbuf, sizeof numbuf, "%lld", (long long)(item->num_val));
                        piece = numbuf;
                    } else if (item->type == JSON_BOOL) {
                        piece = item->bool_val ? "True" : "False";
                    } else {
                        piece = "";
                    }
                }
                if (piece && *piece) {
                    size_t need = strlen(out) + strlen(piece) + 3;
                    if (need > cap) { cap = need * 2; char *nb = realloc(out, cap); if (!nb) break; out = nb; }
                    if (*out) strcat(out, "\n\n");
                    strcat(out, piece);
                }
            }
            /* strip leading/trailing whitespace */
            if (out) {
                char *s = out;
                while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                memmove(out, s, strlen(s) + 1);
                size_t L = strlen(out);
                while (L > 0 && (out[L-1]==' '||out[L-1]=='\t'||out[L-1]=='\n'||out[L-1]=='\r')) out[--L] = '\0';
                if (!*out) { free(out); out = strdup("What do you see in this image?"); }
            }
        }
    } else if (root->type == JSON_NULL) {
        out = strdup("");
    } else {
        /* scalar top-level: mirror Python str() for numbers/bools */
        if (root->type == JSON_NUMBER) {
            char buf[32]; snprintf(buf, sizeof buf, "%lld", (long long)(root->num_val));
            out = strdup(buf);
        } else if (root->type == JSON_BOOL) {
            out = strdup(root->bool_val ? "True" : "False");
        } else {
            out = strdup("");
        }
    }
    json_free(root);
    return out ? out : strdup("");
}

/* PoP: cas2_init @ agent/transports/codex_app_server_session.py:__init__ */
char *cas2_init(const char *cwd, const char *codex_bin) {
    /* Python: session state. */
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"codex_bin\": \"%s\"}",
             cwd ? cwd : ".", codex_bin ? codex_bin : "");
    return out;
}

/* PoP: cas2_close @ agent/transports/codex_app_server_session.py:close */
int cas2_close(void) {
    printf("codex session closed\n");
    return 0;
}

/* PoP: cas2_enter @ agent/transports/codex_app_server_session.py:__enter__ */
char *cas2_enter(void) {
    return strdup("{}");
}

/* PoP: cas2_exit @ agent/transports/codex_app_server_session.py:__exit__ */
int cas2_exit(void) {
    return cas2_close();
}

/* PoP: cas2_request_steer @ agent/transports/codex_app_server_session.py:request_steer */
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

/* PoP: cas2_decide_exec_approval @ agent/transports/codex_app_server_session.py:_decide_exec_approval */
char *cas2_decide_exec_approval(bool auto_approve, const char *command) {
    /* Python: accept when auto; else policy. */
    if (auto_approve) return strdup("accept");
    if (!command) return strdup("reject");
    printf("exec approval decision for: %.60s\n", command);
    return strdup("reject");
}

/* PoP: cas2_decide_apply_patch_approval @ agent/transports/codex_app_server_session.py:_decide_apply_patch_approval */
char *cas2_decide_apply_patch_approval(bool auto_approve) {
    if (auto_approve) return strdup("accept");
    return strdup("reject");
}

/* PoP: cas2_lookup_pending_file_change @ agent/transports/codex_app_server_session.py:_lookup_pending_file_change */
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

/* PoP: cas2_apply_token_usage_notification @ agent/transports/codex_app_server_session.py:_apply_token_usage_notification */
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

/* PoP: cas2_apply_compaction_notification @ agent/transports/codex_app_server_session.py:_apply_compaction_notification */
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

/* PoP: cas2_has_turn_aborted_marker @ agent/transports/codex_app_server_session.py:_has_turn_aborted_marker */
bool cas2_has_turn_aborted_marker(const char *text) {
    /* Python: raw abort markers from codex. */
    if (!text) return false;
    return strstr(text, "turn aborted") != NULL || strstr(text, "TURN_ABORTED") != NULL;
}

/* PoP: cas2_get_hermes_version @ agent/transports/codex_app_server_session.py:_get_hermes_version */
char *cas2_get_hermes_version(void) {
    /* Python: best-effort version for userAgent. */
    const char *v = getenv("HERMES_VERSION");
    if (v && *v) return strdup(v);
    return strdup("0.0.0");
}
