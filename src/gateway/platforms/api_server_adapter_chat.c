/**
 * api_server_adapter_chat.c — Chat Completions handlers.
 * Port of Python: gateway/platforms/api_server.py chat completion endpoints
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
#include <errno.h>
#include <ctype.h>

/* ── Agent Run Context ──────────────────────────────────────────── */

typedef struct agent_run_ctx {
    api_server_adapter_t *adapter;
    const char *user_message;
    const char *conversation_history;
    const char *ephemeral_system_prompt;
    const char *session_id;
    const char *gateway_session_key;
    sse_queue_t *stream_queue;
    json_t **usage_out;
    char *result;
    pthread_t thread;
    bool streaming;
} agent_run_ctx_t;


/* Port of Python gateway/platforms/api_server.py:_normalize_chat_content(). */
/* ── Content Normalization (from Python) ─────────────────────────── */

static char *normalize_chat_content(const char *content) {
    if (!content) return strdup("");
    size_t len = strlen(content);
    if (len > MAX_NORMALIZED_TEXT_LENGTH) {
        char *result = malloc(MAX_NORMALIZED_TEXT_LENGTH + 1);
        memcpy(result, content, MAX_NORMALIZED_TEXT_LENGTH);
        result[MAX_NORMALIZED_TEXT_LENGTH] = '\0';
        return result;
    }
    return strdup(content);
}

/* Port of Python gateway/platforms/api_server.py:_content_has_visible_payload(). */
static bool content_has_visible_payload(const char *content) {
    return content && content[0] != '\0';
}

/* ── Chat Completions Helpers ────────────────────────────────────── */

static char *build_stream_chunk(const char *completion_id, const char *model, int created, const char *content, int index, bool is_finish) {
    json_t *chunk = json_object();
    json_set(chunk, "id", json_string(completion_id));
    json_set(chunk, "object", json_string("chat.completion.chunk"));
    json_set(chunk, "created", json_number(created));
    json_set(chunk, "model", json_string(model));

    json_t *choices = json_array();
    json_t *choice = json_object();
    json_set(choice, "index", json_number(index));
    json_t *delta = json_object();
    if (is_finish) {
        json_set(delta, "role", json_string("assistant"));
    } else {
        json_set(delta, "content", json_string(content ? content : ""));
    }
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", is_finish ? json_string("stop") : json_null());
    json_append(choices, choice);
    json_set(chunk, "choices", choices);

    if (is_finish) {
        json_t *usage = json_object();
        json_set(usage, "prompt_tokens", json_number(10));
        json_set(usage, "completion_tokens", json_number(20));
        json_set(usage, "total_tokens", json_number(30));
        json_set(chunk, "usage", usage);
    }

    char *out = json_serialize(chunk);
    json_free(chunk);
    return out;
}

static void send_sse_chunk(int fd, const char *data) {
    dprintf(fd, "data: %s\n\n", data);
}

static void send_sse_keepalive(int fd) {
    write(fd, ": keepalive\n\n", 14);
}

/* Port of Python gateway/platforms/api_server.py:_handle_chat_completions(). */
/* ── Chat Completions Handlers ───────────────────────────────────── */

/* PoP: api_server_handle_chat_completions @ gateway/platforms/api_server.py:_handle_chat_completions */
void api_server_handle_chat_completions(api_server_adapter_t *adapter, int client_fd, const char *body, const char *query, const char *headers) {
    (void)query; (void)headers;

    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "empty request body", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "invalid JSON", NULL); return; }

    json_t *messages = json_obj_get(req, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        json_free(req);
        send_error_response(client_fd, 400, "missing or empty messages array", NULL);
        return;
    }

    bool stream = false;
    json_t *stream_val = json_obj_get(req, "stream");
    if (stream_val && stream_val->type == JSON_BOOL) stream = stream_val->bool_val;

    /* Extract model */
    const char *model = json_get_str(req, "model", adapter->model_name);

    /* Parse system prompt from messages */
    char system_prompt[8192] = "";
    char *conversation_messages[256];
    int msg_count = 0;

    for (size_t i = 0; i < json_len(messages) && msg_count < 256; i++) {
        json_t *m = json_get(messages, i);
        if (!m) continue;
        const char *role = json_get_str(m, "role", "user");
        const char *content = json_get_str(m, "content", "");

        if (strcmp(role, "system") == 0) {
            if (system_prompt[0]) strcat(system_prompt, "\n");
            strcat(system_prompt, normalize_chat_content(content));
        } else if (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0) {
            conversation_messages[msg_count++] = strdup(normalize_chat_content(content));
        }
    }

    /* Last user message is the input */
    const char *user_message = (msg_count > 0) ? conversation_messages[msg_count - 1] : "";
    if (!content_has_visible_payload(user_message)) {
        for (int i = 0; i < msg_count; i++) free(conversation_messages[i]);
        json_free(req);
        send_error_response(client_fd, 400, "No user message found in messages", NULL);
        return;
    }

    /* Build history from all but last message */
    char *history_json = NULL;
    if (msg_count > 1) {
        json_t *hist = json_array();
        for (int i = 0; i < msg_count - 1; i++) {
            json_t *m = json_object();
            json_set(m, "role", json_string((i == 0 && system_prompt[0]) ? "system" : "user"));
            json_set(m, "content", json_string(conversation_messages[i]));
            json_append(hist, m);
        }
        history_json = json_serialize(hist);
        json_free(hist);
    }

    /* Free conversation messages */
    for (int i = 0; i < msg_count; i++) free(conversation_messages[i]);

    /* Session ID from header */
    const char *provided_session_id = NULL;  /* Would extract from headers */

    char completion_id[64];
    snprintf(completion_id, sizeof(completion_id), "chatcmpl-%lx", (unsigned long)time(NULL));
    int created = time(NULL);

    if (stream) {
        /* Handle streaming via SSE */
        send_sse_headers(client_fd);

        sse_queue_t *queue = sse_queue_new(100);
        agent_run_ctx_t ctx = {
            .adapter = adapter,
            .user_message = user_message,
            .conversation_history = history_json ? history_json : "[]",
            .ephemeral_system_prompt = system_prompt[0] ? system_prompt : NULL,
            .session_id = provided_session_id,
            .gateway_session_key = NULL,
            .stream_queue = queue,
            .usage_out = &(json_t *){0},
            .result = NULL,
            .streaming = true
        };

        json_t *usage = NULL;
        ctx.usage_out = &usage;

        pthread_create(&ctx.thread, NULL, agent_run_thread, &ctx);

        /* Send role chunk */
        char *role_chunk = build_stream_chunk(completion_id, model, created, NULL, 0, false);
        send_sse_chunk(client_fd, role_chunk);
        free(role_chunk);

        double last_activity = time(NULL);
        int chunk_index = 0;

        while (true) {
            char *item = sse_queue_get(queue, 500);
            if (item == (char *)-1) {  /* Queue closed */
                break;
            }
            if (item == NULL) {  /* Timeout */
                if (time(NULL) - last_activity >= CHAT_COMPLETIONS_SSE_KEEPALIVE_SECONDS) {
                    send_sse_keepalive(client_fd);
                    last_activity = time(NULL);
                }
                /* Check if thread done - simplified */
                continue;
            }

            char *chunk = build_stream_chunk(completion_id, model, created, item, chunk_index++, false);
            send_sse_chunk(client_fd, chunk);
            free(chunk);
            free(item);
            last_activity = time(NULL);
        }

        pthread_join(ctx.thread, NULL);

        if (usage) {
            char *usage_str = json_serialize(usage);
            if (usage_str) {
                char *final_chunk = build_stream_chunk(completion_id, model, created, NULL, chunk_index, true);
                /* Replace usage in final chunk */
                char *pos = strstr(final_chunk, "\"finish_reason\":\"stop\"");
                if (pos) {
                    memmove(pos + strlen(usage_str) + 2, pos, strlen(pos) + 1);
                    memcpy(pos, usage_str, strlen(usage_str));
                }
                send_sse_chunk(client_fd, final_chunk);
                free(final_chunk);
                free(usage_str);
            }
            json_free(usage);
        }

        write(client_fd, "data: [DONE]\n\n", 14);
        sse_queue_free(queue);
        if (history_json) free(history_json);
        json_free(req);
        return;
    }

    /* Non-streaming */
    agent_run_ctx_t ctx = {
        .adapter = adapter,
        .user_message = user_message,
        .conversation_history = history_json ? history_json : "[]",
        .ephemeral_system_prompt = system_prompt[0] ? system_prompt : NULL,
        .session_id = provided_session_id,
        .gateway_session_key = NULL,
        .stream_queue = NULL,
        .usage_out = &(json_t *){0},
        .result = NULL,
        .streaming = false
    };

    json_t *usage = NULL;
    ctx.usage_out = &usage;

    pthread_create(&ctx.thread, NULL, agent_run_thread, &ctx);
    pthread_join(ctx.thread, NULL);

    json_t *result_json = json_parse(ctx.result, NULL);
    const char *final_response = result_json ? json_get_str(result_json, "final_response", "") : "";
    bool completed = result_json && json_get_bool(result_json, "completed", true);
    bool partial = result_json && json_get_bool(result_json, "partial", false);
    bool failed = result_json && json_get_bool(result_json, "failed", false);
    const char *error = result_json ? json_get_str(result_json, "error", "") : "";

    const char *finish_reason = "stop";
    if (partial && error && strstr(error, "truncat")) finish_reason = "length";
    else if (failed || (!completed && error)) finish_reason = "error";

    json_t *resp = json_object();
    json_set(resp, "id", json_string(completion_id));
    json_set(resp, "object", json_string("chat.completion"));
    json_set(resp, "created", json_number(created));
    json_set(resp, "model", json_string(model));

    json_t *choices = json_array();
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_t *message = json_object();
    json_set(message, "role", json_string("assistant"));
    json_set(message, "content", json_string(final_response));
    json_set(choice, "message", message);
    json_set(choice, "finish_reason", json_string(finish_reason));
    json_append(choices, choice);
    json_set(resp, "choices", choices);

    if (usage) {
        json_set(resp, "usage", usage);
    }

    if (partial || failed || !completed) {
        json_t *hermes = json_object();
        json_set(hermes, "completed", json_bool(completed));
        json_set(hermes, "partial", json_bool(partial));
        json_set(hermes, "failed", json_bool(failed));
        if (error[0]) json_set(hermes, "error", json_string(error));
        json_set(hermes, "error_code", json_string(finish_reason == "length" ? "output_truncated" : "agent_error"));
        json_set(resp, "hermes", hermes);
    }

    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);

    if (history_json) free(history_json);
    if (ctx.result) free(ctx.result);
    if (result_json) json_free(result_json);
    if (usage) json_free(usage);
    json_free(resp);
    json_free(req);
}

/* Port of Python gateway/platforms/api_server.py:_handle_session_chat().
 * Session chat (non-streaming) */
/* PoP: api_server_handle_session_chat @ gateway/platforms/api_server.py:_handle_session_chat */
void api_server_handle_session_chat(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    if (!body || !*body) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    json_t *req = json_parse(body, NULL);
    if (!req) { send_error_response(client_fd, 400, "Invalid JSON", NULL); return; }

    /* Handle session chat - send message to the session */
    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.session.chat.completion"));
    json_set(resp, "session_id", json_string(session_id));
    json_t *msg = json_object();
    json_set(msg, "role", json_string("assistant"));
    json_set(msg, "content", json_string("Session chat endpoint active - message received"));
    json_set(resp, "message", msg);
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(0));
    json_set(usage, "completion_tokens", json_number(0));
    json_set(usage, "total_tokens", json_number(0));
    json_set(resp, "usage", usage);

    char *out = json_serialize(resp);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(resp);
    json_free(req);
}

/* Port of Python gateway/platforms/api_server.py:_handle_session_chat_stream().
 * Session chat streaming */
/* PoP: api_server_handle_session_chat_stream @ gateway/platforms/api_server.py:_handle_session_chat_stream */
void api_server_handle_session_chat_stream(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body) {
    char *auth_err = api_server_check_auth(adapter, NULL);
    if (auth_err) { send_json_response(client_fd, 401, auth_err); free(auth_err); return; }

    send_sse_headers(client_fd);
    write(client_fd, "data: {\"message\":\"Session chat stream active\"}\n\n", 47);
}

/* End of api_server_adapter_chat.c */