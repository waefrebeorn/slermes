/*
 * codex_app_server_session.c — Session adapter for codex app-server runtime.
 *
 * Owns one Codex thread per Hermes session. Drives turn/start, consumes
 * streaming notifications via CodexEventProjector, handles server-initiated
 * approval requests (apply_patch, exec command), translates cancellation,
 * and returns a clean turn result that the agent loop can splice into
 * its messages list.
 *
 * Maps to Python agent/transports/codex_app_server_session.py (846 lines).
 * Port of Python: codex_app_server_session.py — CodexAppServerSession class methods:
 *   new, free, close, ensure_started, request_interrupt, set_approval_callback,
 *   set_event_callback, run_turn, turn_result_free
 *
 * Port of Python: codex_runtime.py — run_codex_app_server_turn()
 *                 (turn orchestration in codex_session_run_turn)
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "codex_app_server_session.h"
#include "hermes_redact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

/* ================================================================
 *  Constants (mirrors Python module-level constants)
 * ================================================================ */

#define STDERR_TAIL_LINES 12


static const char *OAUTH_HINTS[] = {
    "invalid_grant", "invalid grant", "refresh token", "refresh_token",
    "token refresh", "token_refresh", "token has expired", "expired_token",
    "expired token", "not authenticated", "unauthenticated", "unauthorized",
    "401 unauthorized", "re-authenticate", "reauthenticate",
    "please log in", "please login", "auth profile", "no auth profile",
    "oauth", NULL,
};

/* Turn-aborted markers */
static const char *TURN_ABORTED_MARKERS[] = {
    "<turn_aborted>", "<turn_aborted/>", NULL,
};

/* Pending file-change cache size */
#define MAX_PENDING_FILE_CHANGES 64

/* ================================================================
 *  Session struct
 * ================================================================ */

struct codex_session_t {
    char  *cwd;
    char  *codex_bin;
    char  *codex_home;
    char  *permission_profile;

    codex_client_t *client;
    char  *thread_id;

    /* Interrupt flag — set by request_interrupt, cleared at turn start */
    volatile bool interrupt_requested;
    pthread_mutex_t interrupt_mutex;

    /* Pending file-change items, keyed by item id.
     * Populated on item/started for fileChange items;
     * consumed by the approval bridge. */
    struct {
        char id[128];
        char summary[512];
    } pending_file_changes[MAX_PENDING_FILE_CHANGES];
    int pending_file_change_count;

    /* Callbacks */
    codex_approval_callback_t approval_callback;
    void *approval_user_data;
    codex_event_callback_t event_callback;
    void *event_user_data;

    bool closed;
};

/* ================================================================
 *  Monotonic time (same pattern as llm_client.c)
 * ================================================================ */

static double mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ================================================================
 *  Helpers
 * ================================================================ */

static bool has_turn_aborted_marker(const char *text) {
    if (!text) return false;
    for (int i = 0; TURN_ABORTED_MARKERS[i]; i++) {
        if (strstr(text, TURN_ABORTED_MARKERS[i])) return true;
    }
    return false;
}

/* PoP: classify_oauth_failure @ agent/transports/codex_app_server_session.py:_classify_oauth_failure */
static const char *classify_oauth_failure(const char *s1, const char *s2) {
    /* Build a combined lowercase haystack */
    size_t len = (s1 ? strlen(s1) : 0) + (s2 ? strlen(s2) : 0) + 2;
    char *hay = (char *)malloc(len);
    if (!hay) return NULL;
    hay[0] = '\0';
    if (s1) strcat(hay, s1);
    if (s1 && s2) strcat(hay, " ");
    if (s2) strcat(hay, s2);

    /* Case-insensitive search by lowercasing haystack */
    for (int i = 0; OAUTH_HINTS[i]; i++) {
        if (strcasestr(hay, OAUTH_HINTS[i])) {
            free(hay);
            return "Codex authentication failed — your ChatGPT/Codex login "
                   "looks expired or invalid. Run `codex login` to refresh, "
                   "then retry. (Fall back to default runtime with "
                   "`/codex-runtime auto` if the issue persists.)";
        }
    }
    free(hay);
    return NULL;
}

/* PoP: approval_choice_to_codex_decision @ agent/transports/codex_app_server_session.py:_approval_choice_to_codex_decision */
static const char *approval_choice_to_codex_decision(const char *choice) {
    if (!choice) return "decline";
    if (strcmp(choice, "once") == 0) return "accept";
    if (strcmp(choice, "session") == 0 || strcmp(choice, "always") == 0)
        return "acceptForSession";
    return "decline";
}

/* ---- pending file-change cache ---- */

/* PoP: track_pending_file_change @ agent/transports/codex_app_server_session.py:_track_pending_file_change */
static void track_pending_file_change(codex_session_t *s, const char *notification_json) {
    json_node_t *notif = json_parse(notification_json, NULL);
    if (!notif) return;

    const char *method = json_get_str(notif, "method", "");
    json_node_t *params = json_obj_get(notif, "params");
    if (!params) { json_free(notif); return; }
    json_node_t *item = json_obj_get(params, "item");
    if (!item) { json_free(notif); return; }

    const char *item_type = json_get_str(item, "type", "");
    if (strcmp(item_type, "fileChange") != 0) { json_free(notif); return; }

    const char *item_id = json_get_str(item, "id", "");
    if (!item_id || !item_id[0]) { json_free(notif); return; }

    if (strcmp(method, "item/started") == 0) {
        json_node_t *changes = json_obj_get(item, "changes");
        if (!changes) {
            /* Store placeholder */
            for (int i = 0; i < MAX_PENDING_FILE_CHANGES; i++) {
                if (s->pending_file_changes[i].id[0] == '\0') {
                    strncpy(s->pending_file_changes[i].id, item_id, sizeof(s->pending_file_changes[i].id) - 1);
                    strcpy(s->pending_file_changes[i].summary, "1 change pending");
                    s->pending_file_change_count++;
                    break;
                }
            }
        } else {
            /* Build summary: "3 change(s): path1, path2, +1 more" */
            size_t n = json_len(changes);
            int kinds_count = 0;
            char paths_buf[256] = "";
            int path_count = 0;

            for (size_t i = 0; i < n; i++) {
                json_node_t *ch = json_get(changes, (int)i);
                if (!ch) continue;
                const char *path = json_get_str(ch, "path", "");
                if (path && path[0] && path_count < 3) {
                    if (path_count > 0) strncat(paths_buf, ", ", sizeof(paths_buf) - strlen(paths_buf) - 1);
                    strncat(paths_buf, path, sizeof(paths_buf) - strlen(paths_buf) - 1);
                    path_count++;
                }
                kinds_count++;
            }

            char summary[512];
            if (path_count < (int)n) {
                snprintf(summary, sizeof(summary), "%d change(s): %s, +%zu more",
                         kinds_count, paths_buf, n - path_count);
            } else {
                snprintf(summary, sizeof(summary), "%d change(s): %s",
                         kinds_count, paths_buf);
            }

            for (int i = 0; i < MAX_PENDING_FILE_CHANGES; i++) {
                if (s->pending_file_changes[i].id[0] == '\0') {
                    strncpy(s->pending_file_changes[i].id, item_id, sizeof(s->pending_file_changes[i].id) - 1);
                    s->pending_file_changes[i].id[sizeof(s->pending_file_changes[i].id) - 1] = '\0';
                    snprintf(s->pending_file_changes[i].summary, sizeof(s->pending_file_changes[i].summary), "%s", summary);
                    s->pending_file_change_count++;
                    break;
                }
            }
        }
    } else if (strcmp(method, "item/completed") == 0) {
        for (int i = 0; i < MAX_PENDING_FILE_CHANGES; i++) {
            if (strcmp(s->pending_file_changes[i].id, item_id) == 0) {
                s->pending_file_changes[i].id[0] = '\0';
                s->pending_file_changes[i].summary[0] = '\0';
                s->pending_file_change_count--;
                break;
            }
        }
    }

    json_free(notif);
}

static const char *lookup_pending_file_change(codex_session_t *s, const char *item_id) {
    if (!item_id || !item_id[0]) return NULL;
    for (int i = 0; i < MAX_PENDING_FILE_CHANGES; i++) {
        if (strcmp(s->pending_file_changes[i].id, item_id) == 0)
            return s->pending_file_changes[i].summary;
    }
    return NULL;
}

/* ---- error formatting with stderr tail ---- */

/* PoP: format_error_with_stderr @ agent/transports/codex_app_server_session.py:_format_error_with_stderr */
static char *format_error_with_stderr(codex_session_t *s, const char *prefix,
                                       const char *exc_str, int tail_lines) {
    size_t prefix_len = strlen(prefix);
    size_t exc_len = exc_str ? strlen(exc_str) : 0;
    size_t base_len = prefix_len + (exc_len > 0 ? exc_len + 2 : 0);
    char *base = (char *)malloc(base_len + 1);
    if (!base) return strdup(prefix);
    if (exc_len > 0)
        snprintf(base, base_len + 1, "%s: %s", prefix, exc_str);
    else
        strcpy(base, prefix);

    if (!s->client) return base;

    char *tail = codex_client_stderr_tail(s->client, tail_lines);
    if (!tail || !tail[0] || !tail[0]) { free(tail); return base; }

    /* Redact secrets */
    char *redacted = hermes_redact(tail);
    free(tail);

    size_t result_len = strlen(base) + 32 + strlen(redacted);
    char *result = (char *)malloc(result_len);
    if (!result) { free(base); free(redacted); return strdup(prefix); }
    snprintf(result, result_len, "%s\ncodex stderr (last %d lines):\n%s",
             base, tail_lines, redacted);
    free(base);
    free(redacted);
    return result;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

codex_session_t *codex_session_new(
    const char *cwd,
    const char *codex_bin,
    const char *codex_home,
    const char *permission_profile
) {
    codex_session_t *s = (codex_session_t *)calloc(1, sizeof(codex_session_t));
    if (!s) return NULL;

    s->cwd = cwd ? strdup(cwd) : strdup(".");
    s->codex_bin = codex_bin ? strdup(codex_bin) : strdup("codex");
    s->codex_home = codex_home ? strdup(codex_home) : NULL;

    if (permission_profile) {
        s->permission_profile = strdup(permission_profile);
    } else {
        /* Default: workspace-write */
        s->permission_profile = strdup("workspace-write");
    }

    pthread_mutex_init(&s->interrupt_mutex, NULL);
    s->closed = false;
    return s;
}

void codex_session_free(codex_session_t *s) {
    if (!s) return;
    codex_session_close(s);
    pthread_mutex_destroy(&s->interrupt_mutex);
    free(s->cwd);
    free(s->codex_bin);
    free(s->codex_home);
    free(s->permission_profile);
    free(s);
}

void codex_session_close(codex_session_t *s) {
    if (!s || s->closed) return;
    s->closed = true;
    if (s->client) {
        codex_client_close(s->client);
        codex_client_free(s->client);
        s->client = NULL;
    }
    free(s->thread_id);
    s->thread_id = NULL;
}

void codex_session_set_approval_callback(
    codex_session_t *s,
    codex_approval_callback_t cb,
    void *user_data
) {
    if (!s) return;
    s->approval_callback = cb;
    s->approval_user_data = user_data;
}

void codex_session_set_event_callback(
    codex_session_t *s,
    codex_event_callback_t cb,
    void *user_data
) {
    if (!s) return;
    s->event_callback = cb;
    s->event_user_data = user_data;
}

/* ================================================================
 *  ensure_started — spawn subprocess, initialize, thread/start
 * ================================================================ */

/* PoP: codex_session_ensure_started @ agent/transports/codex_app_server_session.py:ensure_started */
char *codex_session_ensure_started(codex_session_t *s) {
    if (!s) return NULL;
    if (s->thread_id) return strdup(s->thread_id);

    if (!s->client) {
        s->client = codex_client_new(s->codex_bin, s->codex_home, NULL, 0);
        if (!s->client) return NULL;
    }

    /* Initialize handshake */
    int rc = codex_client_initialize(s->client, "hermes", "Hermes Agent", HERMES_VERSION, 15.0);
    if (rc != 0) {
        return NULL;
    }

    /* thread/start */
    char params[1024];
    snprintf(params, sizeof(params), "{\"cwd\":\"%s\"}", s->cwd);
    char *resp = codex_client_request(s->client, "thread/start", params, 15.0);
    if (!resp) return NULL;

    /* Extract thread id — cross-fill thread.id/sessionId/threadId */
    json_node_t *resp_json = json_parse(resp, NULL);
    free(resp);
    if (!resp_json) return NULL;

    json_node_t *thread_obj = json_obj_get(resp_json, "thread");
    char *tid = NULL;

    if (thread_obj) {
        const char *v = json_get_str(thread_obj, "id", NULL);
        if (!v) v = json_get_str(thread_obj, "sessionId", NULL);
        if (v) tid = strdup(v);
    }
    if (!tid) {
        const char *v = json_get_str(resp_json, "sessionId", NULL);
        if (!v) v = json_get_str(resp_json, "threadId", NULL);
        if (v) tid = strdup(v);
    }

    json_free(resp_json);

    if (!tid) {
        /* Failed to get thread id */
        return NULL;
    }

    s->thread_id = tid;
    return strdup(tid);
}

/* ================================================================
 *  Interrupt
 * ================================================================ */
/* PoP: codex_session_request_interrupt @ agent/transports/codex_app_server_session.py:request_interrupt */

void codex_session_request_interrupt(codex_session_t *s) {
    if (!s) return;
    pthread_mutex_lock(&s->interrupt_mutex);
    s->interrupt_requested = true;
    pthread_mutex_unlock(&s->interrupt_mutex);
}

static bool is_interrupt_requested(codex_session_t *s) {
    if (!s) return false;
    pthread_mutex_lock(&s->interrupt_mutex);
    bool v = s->interrupt_requested;
    pthread_mutex_unlock(&s->interrupt_mutex);
    return v;
}

static void clear_interrupt(codex_session_t *s) {
    pthread_mutex_lock(&s->interrupt_mutex);
    s->interrupt_requested = false;
    pthread_mutex_unlock(&s->interrupt_mutex);
}

/* ================================================================
 *  Internal: issue turn/interrupt
 * ================================================================ */

/* PoP: issue_interrupt @ agent/transports/codex_app_server_session.py:_issue_interrupt */
static void issue_interrupt(codex_session_t *s, const char *turn_id) {
    if (!s->client || !s->thread_id || !turn_id) return;
    char params[512];
    snprintf(params, sizeof(params),
             "{\"threadId\":\"%s\",\"turnId\":\"%s\"}", s->thread_id, turn_id);
    codex_client_request(s->client, "turn/interrupt", params, 5.0);
    /* Best-effort: ignore errors */
}

/* ================================================================
 *  Internal: handle server request (approval bridge)
 * ================================================================ */

/* PoP: handle_server_request @ agent/transports/codex_app_server_session.py:_handle_server_request */
static void handle_server_request(codex_session_t *s, const char *req_json) {
    if (!s->client || !req_json) return;

    json_node_t *req = json_parse(req_json, NULL);
    if (!req) return;

    const char *method = json_get_str(req, "method", "");
    json_node_t *id_node = json_obj_get(req, "id");
    /* JSON-RPC id can be string or number */
    char id_str[64] = "";
    if (id_node) {
        if (id_node->type == JSON_STRING) {
            strncpy(id_str, id_node->str_val, sizeof(id_str) - 1);
        } else if (id_node->type == JSON_NUMBER) {
            snprintf(id_str, sizeof(id_str), "%d", (int)id_node->num_val);
        }
    }
    int id_int = atoi(id_str);

    json_node_t *params = json_obj_get(req, "params");
    if (!params) params = json_object(); /* empty fallback */

    if (strcmp(method, "item/commandExecution/requestApproval") == 0) {
        /* Exec approval */
        const char *command = json_get_str(params, "command", "");
        const char *cwd = json_get_str(params, "cwd", NULL);
        if (!cwd || !cwd[0]) cwd = s->cwd;
        const char *reason = json_get_str(params, "reason", NULL);

        char description[1024];
        if (reason) {
            snprintf(description, sizeof(description),
                     "Codex requests exec in %s — %s", cwd, reason);
        } else {
            snprintf(description, sizeof(description),
                     "Codex requests exec in %s", cwd);
        }

        const char *decision = "decline";
        if (s->approval_callback) {
            const char *choice = s->approval_callback(
                command, description, false, s->approval_user_data);
            decision = approval_choice_to_codex_decision(choice);
        }

        char result[64];
        snprintf(result, sizeof(result), "{\"decision\":\"%s\"}", decision);
        codex_client_respond(s->client, id_int, result);

    } else if (strcmp(method, "item/fileChange/requestApproval") == 0) {
        /* Apply-patch approval */
        const char *reason = json_get_str(params, "reason", NULL);
        const char *grant_root = json_get_str(params, "grantRoot", NULL);
        const char *item_id = json_get_str(params, "itemId", "");

        const char *change_summary = lookup_pending_file_change(s, item_id);

        char description[1024];
        int pos = 0;
        if (reason) pos += snprintf(description + pos, sizeof(description) - pos, "%s", reason);
        if (change_summary) {
            if (pos > 0) pos += snprintf(description + pos, sizeof(description) - pos, "; ");
            pos += snprintf(description + pos, sizeof(description) - pos, "%s", change_summary);
        }
        if (grant_root) {
            if (pos > 0) pos += snprintf(description + pos, sizeof(description) - pos, "; ");
            pos += snprintf(description + pos, sizeof(description) - pos, "grants write to %s", grant_root);
        }
        if (pos == 0) strcpy(description, "Codex requests to apply a patch");

        char command_label[512];
        if (change_summary) {
            snprintf(command_label, sizeof(command_label), "apply_patch: %s", change_summary);
        } else if (reason) {
            snprintf(command_label, sizeof(command_label), "apply_patch: %s", reason);
        } else {
            strcpy(command_label, "apply_patch");
        }

        const char *decision = "decline";
        if (s->approval_callback) {
            const char *choice = s->approval_callback(
                command_label, description, false, s->approval_user_data);
            decision = approval_choice_to_codex_decision(choice);
        }

        char result[64];
        snprintf(result, sizeof(result), "{\"decision\":\"%s\"}", decision);
        codex_client_respond(s->client, id_int, result);

    } else if (strcmp(method, "item/permissions/requestApproval") == 0) {
        /* Always decline permission escalations */
        codex_client_respond(s->client, id_int, "{\"decision\":\"decline\"}");

    } else if (strcmp(method, "mcpServer/elicitation/request") == 0) {
        const char *server_name = json_get_str(params, "serverName", "");
        if (strcmp(server_name, "hermes-tools") == 0) {
            codex_client_respond(s->client, id_int,
                "{\"action\":\"accept\",\"content\":null,\"_meta\":null}");
        } else {
            codex_client_respond(s->client, id_int,
                "{\"action\":\"decline\",\"content\":null,\"_meta\":null}");
        }

    } else {
        /* Unknown server request — reject cleanly */
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Unsupported method: %s", method);
        codex_client_respond_error(s->client, id_str, -32601, err_msg);
    }

    json_free(req);
}

/* ================================================================
 *  run_turn — main turn loop
 * ================================================================ */

/* PoP: codex_session_run_turn @ agent/transports/codex_app_server_session.py:run_turn */
codex_turn_result_t *codex_session_run_turn(
    codex_session_t *s,
    const char *user_input,
    double turn_timeout,
    double notification_poll_timeout,
    double post_tool_quiet_timeout
) {
    codex_turn_result_t *result = (codex_turn_result_t *)calloc(1, sizeof(codex_turn_result_t));
    if (!result) return NULL;

    /* Ensure started */
    char *tid = codex_session_ensure_started(s);
    if (!tid) {
        result->error = format_error_with_stderr(s, "codex app-server startup failed", NULL, 12);
        result->should_retire = true;
        return result;
    }
    result->thread_id = tid;

    clear_interrupt(s);

    codex_projector_t *projector = codex_projector_new();

    /* Send turn/start */
    char turn_params[8192];
    snprintf(turn_params, sizeof(turn_params),
             "{\"threadId\":\"%s\",\"input\":[{\"type\":\"text\",\"text\":\"%s\"}]}",
             s->thread_id, user_input);

    /* Escape the user input for JSON — simple approach: the input is already
     * plain text, but we need to escape quotes and backslashes.
     * For now, use the projector's approach: send as-is and rely on
     * the JSON library to handle it. Actually, we need proper escaping.
     * Let's use a simpler approach: build with json_t. */

    /* Rebuild turn params properly with JSON API */
    json_node_t *input_arr = json_array();
    json_node_t *input_item = json_object();
    json_set(input_item, "type", json_string("text"));
    json_set(input_item, "text", json_string(user_input));
    json_append(input_arr, input_item);

    json_node_t *turn_params_obj = json_object();
    json_set(turn_params_obj, "threadId", json_string(s->thread_id));
    json_set(turn_params_obj, "input", input_arr);
    char *turn_params_json = json_serialize(turn_params_obj);
    json_free(turn_params_obj);

    char *turn_resp = codex_client_request(s->client, "turn/start",
                                            turn_params_json, 10.0);
    free(turn_params_json);

    if (!turn_resp) {
        /* Check for OAuth failure */
        char *stderr_blob = codex_client_stderr_tail(s->client, 40);
        const char *hint = classify_oauth_failure(stderr_blob, NULL);
        if (hint) {
            result->error = strdup(hint);
            result->should_retire = true;
        } else {
            result->error = format_error_with_stderr(s, "turn/start failed", NULL, 12);
        }
        free(stderr_blob);
        codex_projector_free(projector);
        return result;
    }

    /* Extract turn id */
    json_node_t *turn_resp_json = json_parse(turn_resp, NULL);
    free(turn_resp);
    if (turn_resp_json) {
        json_node_t *turn_obj = json_obj_get(turn_resp_json, "turn");
        if (turn_obj) {
            const char *tid_str = json_get_str(turn_obj, "id", NULL);
            if (tid_str) result->turn_id = strdup(tid_str);
        }
        json_free(turn_resp_json);
    }

    double deadline = mono_time() + turn_timeout;
    bool turn_complete = false;
    double last_tool_completion_at = -1.0; /* -1 = not armed */

    while (mono_time() < deadline && !turn_complete) {
        if (is_interrupt_requested(s)) {
            issue_interrupt(s, result->turn_id);
            result->interrupted = true;
            break;
        }

        /* Check if subprocess is alive */
        if (!codex_client_is_alive(s->client)) {
            char *stderr_blob = codex_client_stderr_tail(s->client, 60);
            const char *hint = classify_oauth_failure(stderr_blob, NULL);
            if (hint) {
                result->error = strdup(hint);
            } else {
                result->error = format_error_with_stderr(
                    s, "codex app-server subprocess exited unexpectedly",
                    NULL, 20);
            }
            free(stderr_blob);
            result->should_retire = true;
            break;
        }

        /* Post-tool watchdog */
        if (last_tool_completion_at >= 0 &&
            (mono_time() - last_tool_completion_at) > post_tool_quiet_timeout) {
            issue_interrupt(s, result->turn_id);
            result->interrupted = true;
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf),
                     "codex went silent for %.0fs after a tool result; "
                     "retiring app-server session.",
                     post_tool_quiet_timeout);
            result->error = strdup(err_buf);
            result->should_retire = true;
            break;
        }

        /* Drain server-initiated requests (approvals) first */
        char *sreq_json = codex_client_take_server_request(s->client, 0);
        if (sreq_json) {
            /* Drain pending notifications first for up-to-date state */
            for (int i = 0; i < 8; i++) {
                char *pending = codex_client_take_notification(s->client, 0);
                if (!pending) break;
                track_pending_file_change(s, pending);
                codex_projection_t *proj = codex_projector_project(projector, pending);
                if (proj) {
                    for (int j = 0; j < proj->msg_count; j++) {
                        /* Append to result messages */
                        if (result->msg_count >= result->msg_capacity) {
                            int new_cap = result->msg_capacity ? result->msg_capacity * 2 : 8;
                            json_node_t **new_msgs = (json_node_t **)realloc(
                                result->projected_messages, (size_t)new_cap * sizeof(json_node_t *));
                            if (!new_msgs) break;
                            result->projected_messages = new_msgs;
                            result->msg_capacity = new_cap;
                        }
                        result->projected_messages[result->msg_count++] = proj->messages[j];
                        proj->messages[j] = NULL; /* transfer ownership */
                    }
                    if (proj->is_tool_iteration) {
                        result->tool_iterations++;
                        last_tool_completion_at = mono_time();
                    }
                    if (proj->final_text) {
                        free(result->final_text);
                        result->final_text = proj->final_text;
                        proj->final_text = NULL;
                        if (has_turn_aborted_marker(result->final_text)) {
                            turn_complete = true;
                            result->interrupted = true;
                            if (!result->error)
                                result->error = strdup("codex reported turn_aborted");
                        }
                    }
                    codex_projection_free(proj);
                }
                free(pending);
            }
            handle_server_request(s, sreq_json);
            free(sreq_json);
            /* Reset post-tool quiet timer on activity */
            last_tool_completion_at = -1.0;
            continue;
        }

        /* Poll for notifications */
        char *note_json = codex_client_take_notification(s->client, notification_poll_timeout);
        if (!note_json) continue;

        const char *method = "";
        json_node_t *note_parsed = json_parse(note_json, NULL);
        if (note_parsed) {
            method = json_get_str(note_parsed, "method", "");
        }

        /* Event callback */
        if (s->event_callback) {
            s->event_callback(note_json, s->event_user_data);
        }

        /* Track pending file changes */
        track_pending_file_change(s, note_json);

        /* Project into messages */
        codex_projection_t *proj = codex_projector_project(projector, note_json);
        if (proj) {
            for (int j = 0; j < proj->msg_count; j++) {
                if (result->msg_count >= result->msg_capacity) {
                    int new_cap = result->msg_capacity ? result->msg_capacity * 2 : 8;
                    json_node_t **new_msgs = (json_node_t **)realloc(
                        result->projected_messages, (size_t)new_cap * sizeof(json_node_t *));
                    if (!new_msgs) break;
                    result->projected_messages = new_msgs;
                    result->msg_capacity = new_cap;
                }
                result->projected_messages[result->msg_count++] = proj->messages[j];
                proj->messages[j] = NULL;
            }
            if (proj->is_tool_iteration) {
                result->tool_iterations++;
                last_tool_completion_at = mono_time();
            } else {
                if (proj->msg_count > 0 || proj->final_text) {
                    last_tool_completion_at = -1.0;
                }
            }
            if (proj->final_text) {
                free(result->final_text);
                result->final_text = proj->final_text;
                proj->final_text = NULL;
                if (has_turn_aborted_marker(result->final_text)) {
                    turn_complete = true;
                    result->interrupted = true;
                    if (!result->error)
                        result->error = strdup("codex reported turn_aborted");
                }
            }
            codex_projection_free(proj);
        }

        /* Check for turn/completed */
        if (strcmp(method, "turn/completed") == 0) {
            turn_complete = true;
            /* Check turn status for errors */
            if (note_parsed) {
                json_node_t *params = json_obj_get(note_parsed, "params");
                if (params) {
                    json_node_t *turn = json_obj_get(params, "turn");
                    if (turn) {
                        const char *status = json_get_str(turn, "status", "completed");
                        if (strcmp(status, "completed") != 0 &&
                            strcmp(status, "interrupted") != 0) {
                            json_node_t *err_obj = json_obj_get(turn, "error");
                            char err_msg[1024] = "";
                            if (err_obj) {
                                char *err_str = json_serialize(err_obj);
                                if (err_str) {
                                    strncpy(err_msg, err_str, sizeof(err_msg) - 1);
                                    free(err_str);
                                }
                            }
                            /* Check for OAuth failure */
                            char *stderr_blob = codex_client_stderr_tail(s->client, 40);
                            const char *hint = classify_oauth_failure(err_msg, stderr_blob);
                            if (hint) {
                                free(result->error);
                                result->error = strdup(hint);
                                result->should_retire = true;
                            } else {
                                free(result->error);
                                result->error = format_error_with_stderr(
                                    s, err_msg, NULL, 12);
                            }
                            free(stderr_blob);
                        }
                    }
                }
            }
        }

        json_free(note_parsed);
        free(note_json);
    }

    /* Deadline hit without completion */
    if (!turn_complete && !result->interrupted) {
        issue_interrupt(s, result->turn_id);
        result->interrupted = true;
        if (!result->error) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "turn timed out after %.0fs", turn_timeout);
            result->error = format_error_with_stderr(s, err_buf, NULL, 12);
        }
        result->should_retire = true;
    }

    codex_projector_free(projector);
    return result;
}

/* ================================================================
 *  Codex-native thread compaction (faithful port of
 *  agent/transports/codex_app_server_session.py:compact_thread)
 * ================================================================ */

/* PoP: _notification_belongs_to_turn @ agent/transports/codex_app_server_session.py:_notification_belongs_to_turn */
/* True when a notification's scope ids match the active thread/turn. When the
 * caller has not yet captured a turn id (NULL), only the thread id is required.
 * A NULL observed thread id is treated as "belongs" (unspecified = inherits). */
static bool note_belongs_to_turn(codex_session_t *s,
                                 json_node_t *note,
                                 const char *observed_thread_id,
                                 const char *observed_turn_id) {
    (void)note;
    if (observed_thread_id != NULL &&
        strcmp(observed_thread_id, s->thread_id) != 0) {
        return false;
    }
    if (observed_turn_id == NULL) return true;
    /* No active turn yet → any turn id is provisional and accepted. */
    return true;
}

/* PoP: compact_thread @ agent/transports/codex_app_server_session.py:compact_thread */
codex_turn_result_t *codex_session_compact_thread(
    codex_session_t *s,
    double turn_timeout,
    double notification_poll_timeout
) {
    codex_turn_result_t *result = (codex_turn_result_t *)calloc(1, sizeof(codex_turn_result_t));
    if (!result) return NULL;

    /* Ensure started */
    char *tid = codex_session_ensure_started(s);
    if (!tid) {
        result->error = format_error_with_stderr(s, "codex app-server startup failed", NULL, 12);
        result->should_retire = true;
        return result;
    }
    result->thread_id = tid;

    clear_interrupt(s);

    /* thread/compact/start — returns immediately; compaction then streams. */
    char params[256];
    snprintf(params, sizeof(params), "{\"threadId\":\"%s\"}", s->thread_id);

    char *resp = codex_client_request(s->client, "thread/compact/start", params, 10.0);
    if (!resp) {
        char *stderr_blob = codex_client_stderr_tail(s->client, 40);
        const char *hint = classify_oauth_failure(stderr_blob, NULL);
        if (hint) {
            result->error = strdup(hint);
            result->should_retire = true;
        } else {
            result->error = format_error_with_stderr(s, "thread/compact/start failed", NULL, 12);
        }
        free(stderr_blob);
        return result;
    }
    free(resp);

    double deadline = mono_time() + turn_timeout;
    bool turn_complete = false;

    while (mono_time() < deadline && !turn_complete) {
        if (is_interrupt_requested(s)) {
            issue_interrupt(s, result->turn_id);
            result->interrupted = true;
            break;
        }

        if (!codex_client_is_alive(s->client)) {
            char *stderr_blob = codex_client_stderr_tail(s->client, 60);
            const char *hint = classify_oauth_failure(stderr_blob, NULL);
            if (hint) {
                result->error = strdup(hint);
            } else {
                result->error = format_error_with_stderr(
                    s, "codex app-server subprocess exited unexpectedly", NULL, 20);
            }
            free(stderr_blob);
            result->should_retire = true;
            break;
        }

        /* Drain server-initiated requests (approvals) first. */
        char *sreq_json = codex_client_take_server_request(s->client, 0);
        if (sreq_json) {
            handle_server_request(s, sreq_json);
            free(sreq_json);
            continue;
        }

        char *note_json = codex_client_take_notification(s->client, notification_poll_timeout);
        if (!note_json) continue;

        json_node_t *note_parsed = json_parse(note_json, NULL);
        const char *method = note_parsed ? json_get_str(note_parsed, "method", "") : "";
        const char *observed_thread_id = NULL;
        const char *observed_turn_id = NULL;
        if (note_parsed) {
            json_node_t *p = json_obj_get(note_parsed, "params");
            if (p) {
                observed_thread_id = json_get_str(p, "threadId", NULL);
                observed_turn_id = json_get_str(p, "turnId", NULL);
            }
        }

        if (result->turn_id == NULL) {
            if (strcmp(method, "turn/started") == 0) {
                if (observed_thread_id != NULL &&
                    strcmp(observed_thread_id, s->thread_id) != 0) {
                    /* foreign compact turn/started — ignore */
                    json_free(note_parsed);
                    free(note_json);
                    continue;
                }
                if (observed_turn_id == NULL) {
                    json_free(note_parsed);
                    free(note_json);
                    continue;
                }
                result->turn_id = strdup(observed_turn_id);
            } else if (observed_turn_id != NULL ||
                       strcmp(method, "item/completed") == 0 ||
                       strcmp(method, "turn/completed") == 0) {
                /* thread/compact/start does not return a turn id; stale or
                 * unattributable events are ignored until turn/start arrives. */
                json_free(note_parsed);
                free(note_json);
                continue;
            }
        } else if (!note_belongs_to_turn(s, note_parsed, observed_thread_id,
                                         observed_turn_id)) {
            json_free(note_parsed);
            free(note_json);
            continue;
        }

        if (strcmp(method, "turn/started") == 0) {
            if (observed_turn_id) {
                free(result->turn_id);
                result->turn_id = strdup(observed_turn_id);
            }
        } else if (strcmp(method, "turn/completed") == 0) {
            turn_complete = true;
            if (note_parsed) {
                json_node_t *params_n = json_obj_get(note_parsed, "params");
                json_node_t *turn = params_n ? json_obj_get(params_n, "turn") : NULL;
                const char *status = turn ? json_get_str(turn, "status", "completed") : "completed";
                if (strcmp(status, "interrupted") == 0) {
                    result->interrupted = true;
                    result->error = result->error ? result->error
                                                 : strdup("compact turn interrupted");
                } else if (strcmp(status, "completed") != 0) {
                    json_node_t *err_obj = turn ? json_obj_get(turn, "error") : NULL;
                    char err_msg[1024] = "";
                    if (err_obj) {
                        char *err_str = json_serialize(err_obj);
                        if (err_str) {
                            strncpy(err_msg, err_str, sizeof(err_msg) - 1);
                            free(err_str);
                        }
                    }
                    char *stderr_blob = codex_client_stderr_tail(s->client, 40);
                    const char *hint = classify_oauth_failure(err_msg, stderr_blob);
                    if (hint) {
                        result->error = strdup(hint);
                        result->should_retire = true;
                    } else {
                        result->error = format_error_with_stderr(
                            s, err_msg, NULL, 12);
                    }
                    free(stderr_blob);
                }
            }
        }

        json_free(note_parsed);
        free(note_json);
    }

    if (!turn_complete && !result->interrupted) {
        issue_interrupt(s, result->turn_id);
        result->interrupted = true;
        if (!result->error) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf),
                     "compact turn timed out after %.0fs", turn_timeout);
            result->error = format_error_with_stderr(s, err_buf, NULL, 12);
        }
        result->should_retire = true;
    }

    return result;
}

/* ================================================================
 *  Compression-route vtable binding (faithful port of
 *  agent/codex_runtime.py: _record_codex_app_server_compaction +
 *  agent._codex_session ownership)
 *
 *  Python binds a live CodexAppServerSession onto the agent and the
 *  compression route calls agent._codex_session.compact_thread(). The C
 *  compression route (cc_compress_context_via_codex_app_server) carries an
 *  opaque codex-session vtable seam; this facade binds a real
 *  codex_session_t* into that vtable so the route is no longer speculative.
 *  The higher-level codex runtime port fills the remaining agent seams
 *  (build_system_prompt, record_compaction, set_codex_session, …) on the
 *  cc_codex_session_ctx_t it owns; this provides the transport half.
 * ================================================================ */

/* The compression route's vtable type (cc_codex_session_vtable_t) and result
 * type are defined in conversation_compression.h. We bind our transport entry
 * points onto that vtable here. */
#include "conversation_compression.h"
#include "hermes_redact.h"

static cc_codex_compact_result_t codex_route_compact_thunk(void *session) {
    cc_codex_compact_result_t out = { .error = NULL, .interrupted = false,
                                      .should_retire = false };
    codex_session_t *s = (codex_session_t *)session;
    codex_turn_result_t *r = codex_session_compact_thread(s, 600.0, 0.25);
    if (!r) {
        out.error = "codex session compact returned NULL";
        out.should_retire = true;
        return out;
    }
    out.error = r->error;            /* transferred ownership below */
    out.interrupted = r->interrupted;
    out.should_retire = r->should_retire;
    /* take over the malloc'd error string so the caller frees it once */
    r->error = NULL;
    codex_turn_result_free(r);
    return out;
}

static void codex_route_close_thunk(void *session) {
    codex_session_t *s = (codex_session_t *)session;
    codex_session_free(s);
}

/* PoP: _record_codex_app_server_compaction @ agent/codex_runtime.py:_record_codex_app_server_compaction */
/* Bind a live codex session into the compression-route vtable. The caller owns
 * the cc_codex_session_vtable_t storage; this fills only the transport seams. */
void codex_session_bind_compression_vtable(codex_session_t *session,
                                           cc_codex_session_vtable_t *vtab) {
    if (!vtab) return;
    vtab->compact_thread = session ? codex_route_compact_thunk : NULL;
    vtab->close = session ? codex_route_close_thunk : NULL;
}

/* ================================================================
 *  Turn result free
 * ================================================================ */

void codex_turn_result_free(codex_turn_result_t *r) {
    if (!r) return;
    free(r->final_text);
    for (int i = 0; i < r->msg_count; i++) {
        json_free(r->projected_messages[i]);
    }
    free(r->projected_messages);
    free(r->error);
    free(r->turn_id);
    free(r->thread_id);
    free(r);
}
