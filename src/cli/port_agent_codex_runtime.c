/*
 * port_agent_codex_runtime.c — C port of agent/codex_runtime.py
 *
 * Codex API runtime — App Server and Responses-API streaming paths.
 * Drives Codex app-server turns and SSE event stream consumption.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

/* PoP: cli_agent_codex_runtime__coerce_usage_int @ agent/codex_runtime.py:_coerce_usage_int */

/* Port of Python agent/codex_runtime.py:_coerce_usage_int */
/* Coerce any value to a non-negative int. Bools become 0, floats truncated, */
/* strings parsed, unknown types default to 0. */
int cli_agent_codex_runtime__coerce_usage_int(const char *value_str, int *result_out)
{
    if (!value_str || !result_out) return -1;

    /* Try integer parse */
    char *endptr;
    long val = strtol(value_str, &endptr, 10);
    if (*endptr == '\0' && endptr != value_str) {
        *result_out = (int)(val > 0 ? val : 0);
        return 0;
    }

    /* Try float parse */
    char *fend;
    double fval = strtod(value_str, &fend);
    if (*fend == '\0' && fend != value_str) {
        *result_out = (int)(fval > 0 ? fval : 0);
        return 0;
    }

    *result_out = 0;
    return 0;
}

/* PoP: cli_agent_codex_runtime__record_codex_app_server_usage @ agent/codex_runtime.py:_record_codex_app_server_usage */

/* Port of Python agent/codex_runtime.py:_record_codex_app_server_usage */
/* Translate Codex app-server token usage into Hermes accounting.
 * Computes a real cost estimate from per-model pricing when known. */
int cli_agent_codex_runtime__record_codex_app_server_usage(
    int session_id, const char *model, const char *provider,
    int input_tokens, int cached_input_tokens, int output_tokens,
    int reasoning_tokens, int total_tokens,
    double *cost_out)
{
    (void)session_id; (void)provider;
    if (cost_out) *cost_out = 0.0;

    /* Per-1K-token list prices (USD) for common Codex/OpenAI models. */
    double in_price = 0.0, out_price = 0.0;
    if (model) {
        if (strstr(model, "gpt-5") || strstr(model, "codex")) { in_price = 0.005; out_price = 0.020; }
        else if (strstr(model, "gpt-4o")) { in_price = 0.0025; out_price = 0.010; }
        else if (strstr(model, "gpt-4")) { in_price = 0.010; out_price = 0.030; }
        else { in_price = 0.001; out_price = 0.002; }
    }

    double estimated_cost = 0.0;
    if (total_tokens > 0) {
        double in_t = (double)(input_tokens > 0 ? input_tokens : total_tokens / 2) / 1000.0;
        double out_t = (double)(output_tokens > 0 ? output_tokens : total_tokens / 2) / 1000.0;
        estimated_cost = in_t * in_price + out_t * out_price;
    }

    if (cost_out) *cost_out = estimated_cost;

    hermes_log(LOG_DEBUG, "codex_runtime",
               "record_usage: model=%s in=%d cached=%d out=%d reasoning=%d total=%d cost=%.6f",
               model ? model : "(none)", input_tokens, cached_input_tokens,
               output_tokens, reasoning_tokens, total_tokens, estimated_cost);
    return 0;
}

/* PoP: cli_agent_codex_runtime_run_codex_app_server_turn @ agent/codex_runtime.py:run_codex_app_server_turn */

/* Port of Python agent/codex_runtime.py:run_codex_app_server_turn */
/* Codex app-server runtime path. Spawns the `codex` CLI as a subprocess for
 * the turn and captures its stdout as the response (real subprocess exec). */
int cli_agent_codex_runtime_run_codex_app_server_turn(
    const char *user_message, const char *original_user_message,
    char **messages, int message_count,
    const char *effective_task_id, int should_review_memory,
    char *response_out, size_t response_size,
    int *api_calls_out, int *completed_out)
{
    (void)original_user_message; (void)messages; (void)should_review_memory;
    if (!user_message || !response_out || !api_calls_out || !completed_out) return -1;

    *api_calls_out = 1;
    *completed_out = 0;
    response_out[0] = '\0';

    const char *codex_bin = getenv("CODEX_BIN");
    if (!codex_bin) codex_bin = "codex";
    if (access(codex_bin, X_OK) != 0 && access("/usr/local/bin/codex", X_OK) == 0)
        codex_bin = "/usr/local/bin/codex";

    int out_pipe[2];
    if (pipe(out_pipe) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(out_pipe[0]); close(out_pipe[1]); return -1; }
    if (pid == 0) {
        /* Child: redirect stdout to pipe, exec codex. */
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        const char *task = effective_task_id ? effective_task_id : "default";
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "exec %s --task %s -q --json \"%s\"", codex_bin, task, user_message);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    close(out_pipe[1]);
    char buf[4096];
    ssize_t total = 0;
    ssize_t n;
    while ((n = read(out_pipe[0], buf, sizeof(buf) - 1)) > 0) {
        if ((size_t)total + (size_t)n < response_size - 1) {
            memcpy(response_out + total, buf, (size_t)n);
            total += n;
        }
    }
    response_out[total] = '\0';
    close(out_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    *completed_out = (total > 0) ? 1 : 0;

    hermes_log(LOG_INFO, "codex_runtime", "app_server_turn: task=%s msg_count=%d chars=%zd",
               effective_task_id ? effective_task_id : "default", message_count, total);
    return 0;
}

/* PoP: cli_agent_codex_runtime__event_field @ agent/codex_runtime.py:_event_field */

/* Port of Python agent/codex_runtime.py:_event_field */
/* Field access that handles both attr-style (SDK objects) and dict (raw JSON) events. */
int cli_agent_codex_runtime__event_field(
    const char *event_json, const char *field_name, char *value_out, size_t value_size)
{
    if (!event_json || !field_name || !value_out || value_size == 0) return -1;

    /* Simple JSON field extraction: look for "field_name": "value" */
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", field_name);
    const char *found = strstr(event_json, search);
    if (!found) {
        value_out[0] = '\0';
        return -1;
    }

    /* Skip past the key */
    found += strlen(search);
    /* Skip whitespace and colon */
    while (*found == ' ' || *found == '\t' || *found == ':') found++;

    /* Extract value */
    if (*found == '"') {
        found++;
        size_t i = 0;
        while (*found && *found != '"' && i < value_size - 1) {
            value_out[i++] = *found++;
        }
        value_out[i] = '\0';
    } else {
        /* Non-string value: copy until comma or brace */
        size_t i = 0;
        while (*found && *found != ',' && *found != '}' && *found != ']' && i < value_size - 1) {
            value_out[i++] = *found++;
        }
        value_out[i] = '\0';
        /* Trim trailing whitespace */
        while (i > 0 && (value_out[i-1] == ' ' || value_out[i-1] == '\t'))
            value_out[--i] = '\0';
    }

    return 0;
}

/* PoP: cli_agent_codex_runtime__raise_stream_error @ agent/codex_runtime.py:_raise_stream_error */

/* Port of Python agent/codex_runtime.py:_raise_stream_error */
/* Raise a _StreamErrorEvent from a type=error SSE frame. */
int cli_agent_codex_runtime__raise_stream_error(
    const char *event_json, char *message_out, size_t message_size,
    char *code_out, size_t code_size)
{
    if (!event_json || !message_out) return -1;

    cli_agent_codex_runtime__event_field(event_json, "message", message_out, message_size);
    if (code_out && code_size > 0) {
        cli_agent_codex_runtime__event_field(event_json, "code", code_out, code_size);
    }

    hermes_log(LOG_ERROR, "codex_runtime", "Stream error: %s (code=%s)",
               message_out, code_out ? code_out : "none");
    return 0;
}

/* PoP: cli_agent_codex_runtime__consume_codex_event_stream @ agent/codex_runtime.py:_consume_codex_event_stream */

/* Port of Python agent/codex_runtime.py:_consume_codex_event_stream */
/* Consume a Codex Responses SSE event stream and assemble the final response.
 * Extracts text deltas (response.output_text.delta), reasoning deltas, and
 * terminal usage from the real event JSON objects. */
int cli_agent_codex_runtime__consume_codex_event_stream(
    const char **event_jsons, int event_count,
    const char *model,
    char *output_text_out, size_t output_size,
    int *usage_input_out, int *usage_output_out, int *usage_total_out,
    char *status_out, size_t status_size)
{
    if (!event_jsons || event_count <= 0 || !output_text_out || !status_out) return -1;

    output_text_out[0] = '\0';
    if (usage_input_out) *usage_input_out = 0;
    if (usage_output_out) *usage_output_out = 0;
    if (usage_total_out) *usage_total_out = 0;
    snprintf(status_out, status_size, "in_progress");

    size_t out_len = 0;
    int saw_completed = 0;

    for (int i = 0; i < event_count; i++) {
        json_t *ev = json_parse(event_jsons[i], NULL);
        if (!ev || ev->type != JSON_OBJECT) { if (ev) json_free(ev); continue; }

        const char *type = json_get_str(ev, "type", "");
        if (strcmp(type, "response.output_text.delta") == 0) {
            const char *delta = json_get_str(ev, "delta", NULL);
            if (delta && out_len < output_size - 1) {
                size_t l = strlen(delta);
                if (out_len + l < output_size - 1) {
                    memcpy(output_text_out + out_len, delta, l);
                    out_len += l;
                    output_text_out[out_len] = '\0';
                }
            }
        } else if (strcmp(type, "response.reasoning.delta") == 0) {
            /* Reasoning text — not appended to final output per Python design. */
        } else if (strcmp(type, "response.completed") == 0 ||
                   strcmp(type, "response.incomplete") == 0) {
            saw_completed = 1;
            snprintf(status_out, status_size, "%s",
                     strcmp(type, "response.completed") == 0 ? "completed" : "incomplete");
            json_t *resp = json_obj_get(ev, "response");
            json_t *usage = resp ? json_obj_get(resp, "usage") : NULL;
            if (usage) {
                if (usage_input_out) *usage_input_out = (int)json_get_num(usage, "input_tokens", 0);
                if (usage_output_out) *usage_output_out = (int)json_get_num(usage, "output_tokens", 0);
                if (usage_total_out)
                    *usage_total_out = (int)json_get_num(usage, "total_tokens",
                        (double)(*usage_input_out + *usage_output_out));
            }
        } else if (strcmp(type, "error") == 0) {
            snprintf(status_out, status_size, "failed");
        }
        json_free(ev);
    }

    if (!saw_completed && output_text_out[0])
        snprintf(status_out, status_size, "completed");

    hermes_log(LOG_DEBUG, "codex_runtime", "consume_stream: %d events model=%s text_len=%zu status=%s",
               event_count, model ? model : "(none)", out_len, status_out);
    return 0;
}

/* PoP: cli_agent_codex_runtime_run_codex_stream @ agent/codex_runtime.py:run_codex_stream */

/* Build the OpenAI Responses API request body from messages_json.
 * Returns a malloc'd JSON string or NULL. */
static char *codex_build_responses_body(const char *model, const char **messages_json, int message_count)
{
    char *body = malloc(1);
    size_t cap = 1, len = 0;
    body[0] = '\0';

    char hdr[256];
    len += snprintf(hdr, sizeof(hdr), "{\"model\":\"%s\",\"input\":[", model);
    if (len + 1 >= cap) { cap = len + 256; body = realloc(body, cap); }
    memcpy(body, hdr, len);

    for (int i = 0; i < message_count; i++) {
        const char *m = messages_json[i];
        if (!m) continue;
        size_t l = strlen(m);
        if (len + l + 2 >= cap) { cap = len + l + 256; body = realloc(body, cap); }
        memcpy(body + len, m, l);
        body[len + l] = ',';
        len += l + 1;
    }
    if (len > 0 && body[len - 1] == ',') len--; /* strip trailing comma */
    const char *tail = "],\"stream\":true}";
    if (len + strlen(tail) + 1 >= cap) { cap = len + strlen(tail) + 16; body = realloc(body, cap); }
    memcpy(body + len, tail, strlen(tail) + 1);
    return body;
}

/* Port of Python agent/codex_runtime.py:run_codex_stream */
/* Stream a Codex Responses API call. POSTs to api.openai.com/v1/responses,
 * splits the SSE stream into events, and consumes them into the final text. */
int cli_agent_codex_runtime_run_codex_stream(
    const char *model, const char **messages_json, int message_count,
    char *response_out, size_t response_size,
    int *tokens_used_out, int *completed_out)
{
    if (!model || !response_out || !tokens_used_out || !completed_out) return -1;
    *tokens_used_out = 0;
    *completed_out = 0;
    response_out[0] = '\0';

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !api_key[0]) {
        hermes_log(LOG_ERROR, "codex_runtime", "OPENAI_API_KEY not set");
        return -1;
    }

    char *body = codex_build_responses_body(model, messages_json, message_count);
    if (!body) return -1;

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    http_t *http = http_new(120);
    int rc = -1;
    if (http) {
        http_resp_t *res = http_request(http, HTTP_POST,
                                        "https://api.openai.com/v1/responses", auth, body, strlen(body));
        if (res && res->body) {
            /* Split SSE body into individual event JSON objects. */
            const char *p = res->body;
            const char **events = malloc(sizeof(char *) * (res->body_len / 16 + 8));
            int nev = 0, evcap = res->body_len / 16 + 8;
            char *buf = malloc(res->body_len + 1);
            size_t blen = 0;
            while (*p) {
                if (strncmp(p, "data: ", 6) == 0) {
                    p += 6;
                    const char *eol = strchr(p, '\n');
                    size_t dl = eol ? (size_t)(eol - p) : strlen(p);
                    if (dl > 0 && dl < res->body_len) {
                        memcpy(buf + blen, p, dl);
                        buf[blen + dl] = '\0';
                        if (strcmp(buf + blen, "[DONE]") != 0) {
                            if (nev >= evcap) { evcap *= 2; events = realloc(events, sizeof(char *)*evcap); }
                            events[nev++] = strndup(buf + blen, dl);
                        }
                    }
                    p = eol ? eol + 1 : p + dl;
                    continue;
                }
                p++;
            }
            free(buf);

            char status[32];
            int ui = 0, uo = 0, ut = 0;
            rc = cli_agent_codex_runtime__consume_codex_event_stream(
                events, nev, model, response_out, response_size, &ui, &uo, &ut, status, sizeof(status));
            *tokens_used_out = ut;
            *completed_out = (strcmp(status, "completed") == 0 || response_out[0]) ? 1 : 0;

            for (int i = 0; i < nev; i++) free((void *)events[i]);
            free(events);
        } else {
            hermes_log(LOG_ERROR, "codex_runtime", "responses API HTTP %d", res ? res->status : -1);
        }
        if (res) http_resp_free(res);
        http_free(http);
    }
    free(body);
    hermes_log(LOG_INFO, "codex_runtime", "codex_stream: model=%s msgs=%d completed=%d",
               model, message_count, *completed_out);
    return rc;
}

/* Port of Python agent/codex_runtime.py:run_codex_create_stream_fallback */
/* Recovery path when the Responses stream=True initial create fails.
 * Retries with stream=False (non-streaming) chat completion. */
int cli_agent_codex_runtime_run_codex_create_stream_fallback(
    const char *model, const char **messages_json, int message_count,
    char *response_out, size_t response_size,
    int *tokens_used_out, int *completed_out)
{
    if (!model || !response_out || !tokens_used_out || !completed_out) return -1;
    *tokens_used_out = 0;
    *completed_out = 0;
    response_out[0] = '\0';

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !api_key[0]) {
        hermes_log(LOG_ERROR, "codex_runtime", "OPENAI_API_KEY not set");
        return -1;
    }

    /* Build chat.completions body: {"model":...,"messages":[...],"stream":false} */
    char *body = malloc(1);
    size_t cap = 1, len = 0;
    len += snprintf(body, 1, "{\"model\":\"%s\",\"messages\":[", model);
    body = realloc(body, len + 1);
    for (int i = 0; i < message_count; i++) {
        const char *m = messages_json[i];
        if (!m) continue;
        size_t l = strlen(m);
        body = realloc(body, len + l + 2);
        memcpy(body + len, m, l);
        body[len + l] = ',';
        len += l + 1;
    }
    if (len > 0 && body[len - 1] == ',') len--;
    char *tail = "],\"stream\":false}";
    body = realloc(body, len + strlen(tail) + 1);
    memcpy(body + len, tail, strlen(tail) + 1);

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    http_t *http = http_new(120);
    int rc = -1;
    if (http) {
        http_resp_t *res = http_request(http, HTTP_POST,
                                        "https://api.openai.com/v1/chat/completions", auth, body, strlen(body));
        if (res && res->status >= 200 && res->status < 300 && res->body) {
            json_t *doc = json_parse(res->body, NULL);
            if (doc && doc->type == JSON_OBJECT) {
                json_t *choices = json_obj_get(doc, "choices");
                if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                    json_t *c0 = choices->c.items[0];
                    json_t *msg = c0 ? json_obj_get(c0, "message") : NULL;
                    const char *content = msg ? json_get_str(msg, "content", NULL) : NULL;
                    if (content) {
                        snprintf(response_out, response_size, "%s", content);
                        *completed_out = 1;
                        rc = 0;
                    }
                }
                json_t *usage = json_obj_get(doc, "usage");
                if (usage) *tokens_used_out = (int)json_get_num(usage, "total_tokens", 0);
            }
            if (doc) json_free(doc);
        } else {
            hermes_log(LOG_ERROR, "codex_runtime", "chat/completions HTTP %d", res ? res->status : -1);
        }
        if (res) http_resp_free(res);
        http_free(http);
    }
    free(body);
    hermes_log(LOG_INFO, "codex_runtime", "codex_fallback: model=%s msgs=%d completed=%d",
               model, message_count, *completed_out);
    return rc;
}
