/**
 * api_server_adapter_responses.c — OpenAI Responses API handlers.
 * Port of Python: gateway/platforms/api_server.py responses endpoints
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

/* ── Response Store Integration ───────────────────────────────────── */

static char *response_store_get_wrapped(response_store_t *store, const char *response_id) {
    return response_store_get(store, response_id);
}

static void response_store_put_wrapped(response_store_t *store, const char *response_id, const char *data) {
    response_store_put(store, response_id, data);
}

/* ── SSE Helper: Write event to client ────────────────────────────── */

static void sse_write_event_local(int fd, const char *event_type, const char *data) {
    if (event_type) {
        dprintf(fd, "event: %s\n", event_type);
    }
    dprintf(fd, "data: %s\n\n", data ? data : "");
}

/* ── SSE Helper: Send keepalive comment ───────────────────────────── */

static void send_sse_keepalive(int fd) {
    dprintf(fd, ": keepalive\n\n");
}

/* Port of Python gateway/platforms/api_server.py:_handle_responses(). */
/* ── Responses API Handlers ───────────────────────────────────────── */

/* PoP: api_server_handle_responses @ gateway/platforms/api_server.py:_handle_responses */
void api_server_handle_responses(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers) {
    (void)headers;

    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "empty request body", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "invalid JSON", NULL); return; }

    json_t *input = json_obj_get(req, "input");
    if (!input) { json_free(req); send_error_response(client_fd, 400, "Missing 'input' field", NULL); return; }

    const char *instructions = json_get_str(req, "instructions", "");
    const char *previous_response_id = json_get_str(req, "previous_response_id", "");
    const char *conversation = json_get_str(req, "conversation", "");
    bool store = true;
    json_t *store_val = json_obj_get(req, "store");
    if (store_val && store_val->type == JSON_BOOL) store = store_val->bool_val;

    if (conversation[0] && previous_response_id[0]) {
        json_free(req);
        send_error_response(client_fd, 400, "Cannot use both 'conversation' and 'previous_response_id'", NULL);
        return;
    }

    /* Normalize input to messages */
    json_t *messages = json_array();
    if (input->type == JSON_STRING) {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string(input->str_val));
        json_append(messages, msg);
    } else if (input->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(input); i++) {
            json_t *item = json_get(input, i);
            if (item) json_append(messages, item);
        }
    }

    /* Load conversation history from previous_response_id */
    char *history_json = NULL;
    char *stored_session_id = NULL;

    if (conversation[0]) {
        previous_response_id = response_store_get_conversation(adapter->response_store, conversation);
    }

    if (previous_response_id[0]) {
        char *stored = response_store_get(adapter->response_store, previous_response_id);
        if (!stored) {
            json_free(messages);
            json_free(req);
            send_error_response(client_fd, 404, "Previous response not found", NULL);
            return;
        }
        json_t *stored_json = json_parse(stored, NULL);
        if (stored_json) {
            json_t *hist = json_obj_get(stored_json, "conversation_history");
            if (hist) history_json = json_serialize(hist);
            if (!instructions[0]) instructions = json_get_str(stored_json, "instructions", "");
            stored_session_id = strdup(json_get_str(stored_json, "session_id", ""));
            json_free(stored_json);
        }
        free(stored);
    }

    /* Last input is user message */
    const char *user_message = "";
    if (json_len(messages) > 0) {
        json_t *last = json_get(messages, json_len(messages) - 1);
        user_message = json_get_str(last, "content", "");
    }

    if (!api_server_content_has_visible_payload(user_message)) {
        json_free(messages);
        if (history_json) free(history_json);
        if (stored_session_id) free(stored_session_id);
        json_free(req);
        send_error_response(client_fd, 400, "No user message found in input", NULL);
        return;
    }

    /* Session ID */
    char session_id[128];
    if (stored_session_id) {
        strncpy(session_id, stored_session_id, sizeof(session_id) - 1);
        free(stored_session_id);
    } else {
        char *uuid_str = uuid_v4();
        if (!uuid_str) uuid_str = strdup("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        strncpy(session_id, uuid_str, sizeof(session_id) - 1);
        free(uuid_str);
    }

    bool stream = false;
    json_t *stream_val = json_obj_get(req, "stream");
    if (stream_val && stream_val->type == JSON_BOOL) stream = stream_val->bool_val;

    const char *model = json_get_str(req, "model", adapter->model_name);
    char response_id[64];
    time_t created_at = time(NULL);
    snprintf(response_id, sizeof(response_id), "resp_%lx", (unsigned long)created_at);

    if (stream) {
        /* Streaming responses - SSE with OpenAI Responses events */
        send_sse_headers(client_fd);

        sse_queue_t *queue = sse_queue_new(100);

        agent_run_args_t run_args = {
            .user_message = user_message,
            .conversation_history = history_json ? history_json : "[]",
            .ephemeral_system_prompt = instructions[0] ? instructions : NULL,
            .session_id = session_id,
            .gateway_session_key = NULL,
            .usage_out = &(json_t *){0},
            .result = NULL
        };

        json_t *usage = NULL;
        run_args.usage_out = &usage;

        pthread_t thread;
        pthread_create(&thread, NULL, agent_run_thread, &run_args);

        /* response.created event */
        json_t *created_env = json_object();
        json_set(created_env, "id", json_string(response_id));
        json_set(created_env, "object", json_string("response"));
        json_set(created_env, "status", json_string("in_progress"));
        json_set(created_env, "created_at", json_number(created_at));
        json_set(created_env, "model", json_string(model));
        json_set(created_env, "output", json_array());

        char *created_str = json_serialize(created_env);
        sse_write_event_local(client_fd, "response.created", created_str);

        if (store) {
            response_store_put(adapter->response_store, response_id, created_str);
        }

        free(created_str);
        json_free(created_env);

        int output_index = 0;
        char message_item_id[64];
        char *uuid_str = uuid_v4();
        if (!uuid_str) uuid_str = strdup("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        strncpy(message_item_id, uuid_str, sizeof(message_item_id) - 1);
        free(uuid_str);
        message_item_id[3] = '_';  /* msg_ prefix */
        int message_output_index = -1;
        bool message_opened = false;

        double last_activity = time(NULL);

        while (true) {
            char *item = sse_queue_get(queue, 500);
            if (item == (char *)-1) break;
            if (item == NULL) {
                if (time(NULL) - last_activity >= CHAT_COMPLETIONS_SSE_KEEPALIVE_SECONDS) {
                    send_sse_keepalive(client_fd);
                    last_activity = time(NULL);
                }
                continue;
            }

            /* For now, treat each item as text delta */
            if (!message_opened) {
                message_opened = true;
                message_output_index = output_index++;
                json_t *added = json_object();
                json_set(added, "id", json_string(message_item_id));
                json_set(added, "type", json_string("message"));
                json_set(added, "status", json_string("in_progress"));
                json_set(added, "role", json_string("assistant"));
                json_set(added, "content", json_array());
                json_t *evt = json_object();
                json_set(evt, "type", json_string("response.output_item.added"));
                json_set(evt, "output_index", json_number(message_output_index));
                json_set(evt, "item", added);
                char *evt_str = json_serialize(evt);
                sse_write_event_local(client_fd, "response.output_item.added", evt_str);
                free(evt_str);
                json_free(evt);
            }

            json_t *delta = json_object();
            json_set(delta, "type", json_string("response.output_text.delta"));
            json_set(delta, "item_id", json_string(message_item_id));
            json_set(delta, "output_index", json_number(message_output_index));
            json_set(delta, "content_index", json_number(0));
            json_set(delta, "delta", json_string(item));
            json_set(delta, "logprobs", json_array());
            char *delta_str = json_serialize(delta);
            sse_write_event_local(client_fd, "response.output_text.delta", delta_str);
            free(delta_str);
            json_free(delta);
            free(item);
            last_activity = time(NULL);
        }

        pthread_join(thread, NULL);

        /* response.output_text.done */
        if (message_opened) {
            json_t *done = json_object();
            json_set(done, "type", json_string("response.output_text.done"));
            json_set(done, "item_id", json_string(message_item_id));
            json_set(done, "output_index", json_number(message_output_index));
            json_set(done, "content_index", json_number(0));
            json_set(done, "text", json_string("Final response text"));
            json_set(done, "logprobs", json_array());
            char *done_str = json_serialize(done);
            sse_write_event_local(client_fd, "response.output_text.done", done_str);
            free(done_str);
            json_free(done);

            json_t *msg_done = json_object();
            json_set(msg_done, "id", json_string(message_item_id));
            json_set(msg_done, "type", json_string("message"));
            json_set(msg_done, "status", json_string("completed"));
            json_set(msg_done, "role", json_string("assistant"));
            json_t *content = json_array();
            json_t *text_item = json_object();
            json_set(text_item, "type", json_string("output_text"));
            json_set(text_item, "text", json_string("Final response text"));
            json_append(content, text_item);
            json_set(msg_done, "content", content);
            json_t *evt = json_object();
            json_set(evt, "type", json_string("response.output_item.done"));
            json_set(evt, "output_index", json_number(message_output_index));
            json_set(evt, "item", msg_done);
            char *evt_str = json_serialize(evt);
            sse_write_event_local(client_fd, "response.output_item.done", evt_str);
            free(evt_str);
            json_free(evt);
        }

        /* response.completed */
        json_t *completed = json_object();
        json_set(completed, "id", json_string(response_id));
        json_set(completed, "object", json_string("response"));
        json_set(completed, "status", json_string("completed"));
        json_set(completed, "created_at", json_number(created_at));
        json_set(completed, "model", json_string(model));
        json_t *out_items = json_array();
        json_t *out_msg = json_object();
        json_set(out_msg, "type", json_string("message"));
        json_set(out_msg, "role", json_string("assistant"));
        json_t *out_content = json_array();
        json_t *out_text = json_object();
        json_set(out_text, "type", json_string("output_text"));
        json_set(out_text, "text", json_string("Final response text"));
        json_append(out_content, out_text);
        json_set(out_msg, "content", out_content);
        json_append(out_items, out_msg);
        json_set(completed, "output", out_items);
        if (usage) json_set(completed, "usage", usage);

        char *completed_str = json_serialize(completed);
        sse_write_event_local(client_fd, "response.completed", completed_str);

        if (store) {
            response_store_put(adapter->response_store, response_id, completed_str);
        }

        free(completed_str);
        json_free(completed);
        if (usage) json_free(usage);
        sse_queue_free(queue);
        if (history_json) free(history_json);
        json_free(messages);
        json_free(req);
        return;
    }

    /* Non-streaming responses */
    agent_run_args_t run_args = {
        .user_message = user_message,
        .conversation_history = history_json ? history_json : "[]",
        .ephemeral_system_prompt = instructions[0] ? instructions : NULL,
        .session_id = session_id,
        .gateway_session_key = NULL,
        .usage_out = &(json_t *){0},
        .result = NULL
    };

    json_t *usage = NULL;
    run_args.usage_out = &usage;

    pthread_t thread;
    pthread_create(&thread, NULL, agent_run_thread, &run_args);
    pthread_join(thread, NULL);

    json_t *result_json = json_parse(run_args.result, NULL);
    const char *final_response = result_json ? json_get_str(result_json, "final_response", "") : "";

    char response_id_ns[64];
    snprintf(response_id_ns, sizeof(response_id_ns), "resp_%lx", (unsigned long)time(NULL));

    json_t *resp = json_object();
    json_set(resp, "id", json_string(response_id_ns));
    json_set(resp, "object", json_string("response"));
    json_set(resp, "status", json_string("completed"));
    json_set(resp, "created_at", json_number(time(NULL)));
    json_set(resp, "model", json_string(model));

    json_t *output = json_array();
    if (final_response[0]) {
        json_t *msg = json_object();
        json_set(msg, "type", json_string("message"));
        json_set(msg, "role", json_string("assistant"));
        json_t *content = json_array();
        json_t *text = json_object();
        json_set(text, "type", json_string("output_text"));
        json_set(text, "text", json_string(final_response));
        json_append(content, text);
        json_set(msg, "content", content);
        json_append(output, msg);
    }
    json_set(resp, "output", output);

    if (usage) json_set(resp, "usage", usage);

    char *resp_str = json_serialize(resp);
    send_json_response(client_fd, 200, resp_str);

    if (store) {
        json_t *to_store = json_object();
        json_set(to_store, "response", resp);
        json_set(to_store, "conversation_history", json_parse(history_json ? history_json : "[]", NULL));
        json_set(to_store, "instructions", json_string(instructions));
        json_set(to_store, "session_id", json_string(session_id));
        char *store_str = json_serialize(to_store);
        response_store_put(adapter->response_store, response_id_ns, store_str);
        free(store_str);
        json_free(to_store);

        if (conversation[0]) {
            response_store_set_conversation(adapter->response_store, conversation, response_id_ns);
        }
    }

    free(resp_str);
    if (result_json) json_free(result_json);
    if (usage) json_free(usage);
    if (run_args.result) free(run_args.result);
    if (history_json) free(history_json);
    json_free(resp);
    json_free(messages);
    json_free(req);
}

/* Port of Python gateway/platforms/api_server.py:_handle_get_response(). */
/* PoP: api_server_handle_get_response @ gateway/platforms/api_server.py:_handle_get_response */
void api_server_handle_get_response(api_server_adapter_t *adapter, int client_fd, const char *response_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    char *stored = response_store_get(adapter->response_store, response_id);
    if (!stored) { send_error_response(client_fd, 404, "Response not found", NULL); return; }

    json_t *stored_json = json_parse(stored, NULL);
    if (stored_json) {
        json_t *resp = json_obj_get(stored_json, "response");
        if (resp) {
            char *out = json_serialize(resp);
            send_json_response(client_fd, 200, out);
            free(out);
        } else {
            send_error_response(client_fd, 500, "Invalid response format", NULL);
        }
        json_free(stored_json);
    } else {
        send_error_response(client_fd, 500, "Failed to parse stored response", NULL);
    }
    free(stored);
}

/* Port of Python gateway/platforms/api_server.py:_handle_delete_response(). */
/* PoP: api_server_handle_delete_response @ gateway/platforms/api_server.py:_handle_delete_response */
void api_server_handle_delete_response(api_server_adapter_t *adapter, int client_fd, const char *response_id) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    bool deleted = response_store_delete(adapter->response_store, response_id);
    if (!deleted) { send_error_response(client_fd, 404, "Response not found", NULL); return; }

    json_t *resp = json_object();
    json_set(resp, "id", json_string(response_id));
    json_set(resp, "object", json_string("response"));
    json_set(resp, "deleted", json_bool(true));
    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(resp);
}

/* End of api_server_adapter_responses.c */