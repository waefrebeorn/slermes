/**
 * api_server_adapter_runs.c — Structured event streaming (runs) handlers.
 * Port of Python: gateway/platforms/api_server.py /v1/runs endpoints
 */

#include "api_server_adapter.h"
#include "hermes_gateway_webhook.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "uuid.h"
#include <pthread.h>

/* ── Run Thread Context ──────────────────────────────────────────── */

typedef struct run_thread_ctx {
    api_server_adapter_t *adapter;
    run_status_t *run_status;
    const char *user_message;
    const char *conversation_history;
    const char *instructions;
    const char *session_id;
    const char *gateway_session_key;
    const char *model;
    pthread_t run_thread;
} run_thread_ctx_t;

static void *run_thread_fn(void *arg) {
    run_thread_ctx_t *ctx = (run_thread_ctx_t *)arg;
    run_status_t *rs = ctx->run_status;
    api_server_adapter_t *adapter = ctx->adapter;

    /* Send run.started event */
    json_t *started = json_object();
    json_set(started, "event", json_string("run.started"));
    json_set(started, "run_id", json_string(rs->run_id));
    json_set(started, "timestamp", json_number(time(NULL)));
    json_set(started, "user_message", json_string(ctx->user_message));
    char *started_str = json_serialize(started);
    run_status_send_event(rs, "run.started", started_str);
    free(started_str);
    json_free(started);

    run_status_update(rs, "running", started_str);

    /* Run agent using shared API */
    agent_run_args_t actx = {
        .user_message = ctx->user_message,
        .conversation_history = ctx->conversation_history,
        .ephemeral_system_prompt = ctx->instructions,
        .session_id = ctx->session_id,
        .gateway_session_key = ctx->gateway_session_key,
        .usage_out = &(json_t *){0},
        .result = NULL
    };

    json_t *usage = NULL;
    actx.usage_out = &usage;

    pthread_t run_thread;
    pthread_create(&run_thread, NULL, agent_run_thread, &actx);
    pthread_join(run_thread, NULL);

    json_t *result_json = json_parse(actx.result, NULL);
    const char *final_response = result_json ? json_get_str(result_json, "final_response", "") : "";
    bool failed = result_json && json_get_bool(result_json, "failed", false);
    const char *error = result_json ? json_get_str(result_json, "error", "") : "";

    if (failed) {
        json_t *failed_evt = json_object();
        json_set(failed_evt, "event", json_string("run.failed"));
        json_set(failed_evt, "run_id", json_string(rs->run_id));
        json_set(failed_evt, "timestamp", json_number(time(NULL)));
        json_set(failed_evt, "error", json_string(error));
        char *failed_str = json_serialize(failed_evt);
        run_status_send_event(rs, "run.failed", failed_str);
        free(failed_str);
        json_free(failed_evt);
        run_status_update(rs, "failed", failed_str);
    } else {
        json_t *completed = json_object();
        json_set(completed, "event", json_string("run.completed"));
        json_set(completed, "run_id", json_string(rs->run_id));
        json_set(completed, "timestamp", json_number(time(NULL)));
        json_set(completed, "output", json_string(final_response));
        if (usage) {
            json_t *usage_copy = json_object();
            json_set(usage_copy, "prompt_tokens", json_obj_get(usage, "input_tokens"));
            json_set(usage_copy, "completion_tokens", json_obj_get(usage, "output_tokens"));
            json_set(usage_copy, "total_tokens", json_obj_get(usage, "total_tokens"));
            json_set(completed, "usage", usage_copy);
        }
        char *completed_str = json_serialize(completed);
        run_status_send_event(rs, "run.completed", completed_str);
        free(completed_str);
        json_free(completed);
        run_status_update(rs, "completed", completed_str);
    }

    free(rs->output);
    rs->output = final_response ? strdup(final_response) : NULL;
    if (usage) rs->usage = usage;

    /* Send sentinel */
    sse_queue_close(rs->event_queue);
    rs->running = false;

    if (result_json) json_free(result_json);
    if (actx.result) free(actx.result);
    free(ctx);
    return NULL;
}

/* ── SSE Helper: Send keepalive comment ───────────────────────────── */

static void send_sse_keepalive(int fd) {
    dprintf(fd, ": keepalive\n\n");
}

/* Port of Python gateway/platforms/api_server.py:_handle_runs(). */
/* ── Runs Handlers ───────────────────────────────────────────────── */

/* PoP: api_server_handle_runs @ gateway/platforms/api_server.py:_handle_runs */
void api_server_handle_runs(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers) {
    (void)headers;

    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "empty request body", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "invalid JSON", NULL); return; }

    json_t *input = json_obj_get(req, "input");
    if (!input) { json_free(req); send_error_response(client_fd, 400, "Missing 'input' field", NULL); return; }

    const char *user_message = input->type == JSON_STRING ? input->str_val : "";
    if (!api_server_content_has_visible_payload(user_message)) {
        json_free(req);
        send_error_response(client_fd, 400, "No user message found in input", NULL);
        return;
    }

    const char *instructions = json_get_str(req, "instructions", "");
    const char *previous_response_id = json_get_str(req, "previous_response_id", "");

    /* Load conversation history */
    char *history_json = NULL;
    if (previous_response_id[0]) {
        char *stored = response_store_get(adapter->response_store, previous_response_id);
        if (stored) {
            json_t *stored_json = json_parse(stored, NULL);
            if (stored_json) {
                json_t *hist = json_obj_get(stored_json, "conversation_history");
                if (hist) history_json = json_serialize(hist);
                if (!instructions[0]) instructions = json_get_str(stored_json, "instructions", "");
            }
            json_free(stored_json);
            free(stored);
        }
    }

    /* Check concurrency */
    int active_runs = 0;
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        if (strcmp(adapter->run_statuses[i]->status, "running") == 0) active_runs++;
    }
    if (active_runs >= MAX_CONCURRENT_RUNS) {
        pthread_mutex_unlock(&adapter->run_lock);
        json_free(req);
        if (history_json) free(history_json);
        send_error_response(client_fd, 429, "Too many concurrent runs", "rate_limit_exceeded");
        return;
    }
    pthread_mutex_unlock(&adapter->run_lock);

    char run_id[64];
    snprintf(run_id, sizeof(run_id), "run_%lx", (unsigned long)time(NULL));
    const char *session_id = json_get_str(req, "session_id", "");
    if (!session_id[0]) session_id = run_id;

    char approval_session_key[128];
    snprintf(approval_session_key, sizeof(approval_session_key), "%s:%s", "api_server", session_id);

    const char *model = json_get_str(req, "model", adapter->model_name);

    /* Create run status using header API */
    run_status_t *rs = run_status_new(run_id, session_id, model);
    strncpy(rs->approval_session_key, approval_session_key, sizeof(rs->approval_session_key) - 1);

    run_thread_ctx_t *ctx = malloc(sizeof(run_thread_ctx_t));
    ctx->adapter = adapter;
    ctx->run_status = rs;
    ctx->user_message = user_message;
    ctx->conversation_history = history_json ? history_json : "[]";
    ctx->instructions = instructions;
    ctx->session_id = session_id;
    ctx->gateway_session_key = NULL;  /* Would extract from headers */
    ctx->model = model;

    run_status_set(adapter, run_id, rs);

    pthread_create(&ctx->run_thread, NULL, run_thread_fn, ctx);

    char run_thread_id[32];
    snprintf(run_thread_id, sizeof(run_thread_id), "%lx", (unsigned long)ctx->run_thread);
    rs->run_thread = ctx->run_thread;

    json_t *resp = json_object();
    json_set(resp, "run_id", json_string(run_id));
    json_set(resp, "status", json_string("started"));
    char *out = json_serialize(resp);
    send_json_response(client_fd, 202, out);
    free(out);
    json_free(resp);

    json_free(req);
    if (history_json) free(history_json);
}

/* Port of Python gateway/platforms/api_server.py:_handle_get_run(). */
/* PoP: api_server_handle_get_run @ gateway/platforms/api_server.py:_handle_get_run */
void api_server_handle_get_run(api_server_adapter_t *adapter, int client_fd, const char *run_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    run_status_t *rs = run_status_get(adapter, run_id);
    if (!rs) { send_error_response(client_fd, 404, "Run not found", "run_not_found"); return; }

    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.run"));
    json_set(resp, "run_id", json_string(rs->run_id));
    json_set(resp, "status", json_string(rs->status));
    json_set(resp, "created_at", json_number(rs->created_at));
    json_set(resp, "updated_at", json_number(rs->updated_at));
    if (rs->session_id[0]) json_set(resp, "session_id", json_string(rs->session_id));
    if (rs->model[0]) json_set(resp, "model", json_string(rs->model));
    if (rs->output) json_set(resp, "output", json_string(rs->output));
    if (rs->error) json_set(resp, "error", json_string(rs->error));
    if (rs->usage) json_set(resp, "usage", rs->usage);
    if (rs->last_event_json) json_set(resp, "last_event", json_string(rs->last_event_json));

    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(resp);
}

/* Port of Python gateway/platforms/api_server.py:_handle_run_events(). */
/* PoP: api_server_handle_run_events @ gateway/platforms/api_server.py:_handle_run_events */
void api_server_handle_run_events(api_server_adapter_t *adapter, int client_fd, const char *run_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    run_status_t *rs = run_status_get(adapter, run_id);
    if (!rs) { send_error_response(client_fd, 404, "Run not found", "run_not_found"); return; }

    send_sse_headers(client_fd);

    double last_activity = time(NULL);
    while (true) {
        char *item = sse_queue_get(rs->event_queue, 1000);
        if (item == (char *)-1) {  /* Queue closed */
            write(client_fd, ": stream closed\n\n", 17);
            break;
        }
        if (item == NULL) {  /* Timeout */
            if (time(NULL) - last_activity >= 30) {
                send_sse_keepalive(client_fd);
                last_activity = time(NULL);
            }
            continue;
        }
        write(client_fd, item, strlen(item));
        free(item);
        last_activity = time(NULL);
    }

    run_status_remove(adapter, run_id);
}

/* Port of Python gateway/platforms/api_server.py:_handle_run_approval(). */
/* PoP: api_server_handle_run_approval @ gateway/platforms/api_server.py:_handle_run_approval */
void api_server_handle_run_approval(api_server_adapter_t *adapter, int client_fd, const char *run_id, const char *body) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    run_status_t *rs = run_status_get(adapter, run_id);
    if (!rs) { send_error_response(client_fd, 404, "Run not found", "run_not_found"); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    const char *choice = json_get_str(req, "choice", "");
    const char *aliases[][2] = {
        {"approve", "once"}, {"approved", "once"}, {"allow", "once"}, {NULL, NULL}
    };
    for (int i = 0; aliases[i][0]; i++) {
        if (strcmp(choice, aliases[i][0]) == 0) { choice = aliases[i][1]; break; }
    }

    const char *valid[] = {"once", "session", "always", "deny", NULL};
    bool ok = false;
    for (int i = 0; valid[i]; i++) if (strcmp(choice, valid[i]) == 0) { ok = true; break; }
    if (!ok) {
        json_free(req);
        send_error_response(client_fd, 400, "Invalid approval choice", "invalid_approval_choice");
        return;
    }

    bool resolve_all = false;
    json_t *all_val = json_obj_get(req, "all");
    if (all_val && all_val->type == JSON_BOOL) resolve_all = all_val->bool_val;
    json_t *ra_val = json_obj_get(req, "resolve_all");
    if (ra_val && ra_val->type == JSON_BOOL) resolve_all = ra_val->bool_val;
    json_free(req);

    /* Would call resolve_gateway_approval from tools/approval.c */
    int resolved = 0;  /* Placeholder */

    if (resolved <= 0) {
        send_error_response(client_fd, 409, "Run has no pending approval", "approval_not_pending");
        return;
    }

    run_status_update(rs, "running", NULL);

    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.run.approval_response"));
    json_set(resp, "run_id", json_string(run_id));
    json_set(resp, "choice", json_string(choice));
    json_set(resp, "resolved", json_number(resolved));
    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(resp);

    /* Send approval.responded event */
    json_t *evt = json_object();
    json_set(evt, "event", json_string("approval.responded"));
    json_set(evt, "run_id", json_string(run_id));
    json_set(evt, "timestamp", json_number(time(NULL)));
    json_set(evt, "choice", json_string(choice));
    json_set(evt, "resolved", json_number(resolved));
    char *evt_str = json_serialize(evt);
    run_status_send_event(rs, "approval.responded", evt_str);
    free(evt_str);
    json_free(evt);
}

/* Port of Python gateway/platforms/api_server.py:_handle_stop_run(). */
/* PoP: api_server_handle_stop_run @ gateway/platforms/api_server.py:_handle_stop_run */
void api_server_handle_stop_run(api_server_adapter_t *adapter, int client_fd, const char *run_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    run_status_t *rs = run_status_get(adapter, run_id);
    if (!rs) { send_error_response(client_fd, 404, "Run not found", "run_not_found"); return; }

    run_status_update(rs, "stopping", NULL);

    /* Signal thread to stop */
    sse_queue_close(rs->event_queue);

    json_t *resp = json_object();
    json_set(resp, "run_id", json_string(run_id));
    json_set(resp, "status", json_string("stopping"));
    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(resp);
}

/* End of api_server_adapter_runs.c */