/*
 * llm_stream.c — streaming chunk-callback handlers.
 *
 * Split from llm_client.c (soul.md: self-contained modules). Handles
 * per-chunk parsing, tool-call accumulation, and stream diagnostics for
 * both provider-aware and OpenAI-style streams. No file-scope state.
 */
#include "llm_stream.h"
#include "hermes_json.h"
#include "hermes_clock.h"
#include "error_classifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

int on_provider_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    if (!ctx->prov_ops || !ctx->prov) {
        /* No provider — skip */
        return 0;
    }

    /* Try provider's parse_stream_chunk first */
    provider_response_t *delta = ctx->prov_ops->parse_stream_chunk(ctx->prov, data);
    if (delta) {
        /* Text content from provider */
        if (delta->content && delta->content[0]) {
            /* P95: Track first token timing */
            if (!ctx->first_token_flag) {
                ctx->first_token_flag = true;
                ctx->resp->diag.first_token_time = clock_mono_sec();
                ctx->resp->diag.first_token_received = true;
                ctx->resp->diag.time_to_first_token =
                    ctx->resp->diag.first_token_time - ctx->req_start_time;
            }
            ctx->token_count++;
            size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
            size_t add = strlen(delta->content);
            char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
            if (newc) {
                ctx->resp->content = newc;
                memcpy(ctx->resp->content + cur, delta->content, add + 1);
            }
            if (ctx->token_cb) {
                int cb_ret = ctx->token_cb(delta->content, ctx->userdata);
                if (cb_ret != 0) {
                    /* Caller requested abort (e.g. user interrupt) — stop streaming */
                    ctx->prov_ops->free_response(delta);
                    return 1;
                }
            }
        }

        /* B22: finish_reason from provider delta */
        if (delta->finish_reason[0])
            snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason),
                     "%s", delta->finish_reason);

        /* Token counts from delta (Anthropic sends in message_delta) */
        if (delta->input_tokens > 0)
            ctx->resp->input_tokens = delta->input_tokens;
        if (delta->output_tokens > 0)
            ctx->resp->output_tokens = delta->output_tokens;

        /* Accumulate reasoning content across streaming chunks */
        if (delta->reasoning && delta->reasoning[0]) {
            size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
            size_t add = strlen(delta->reasoning);
            char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
            if (newr) {
                ctx->resp->reasoning = newr;
                memcpy(ctx->resp->reasoning + cur, delta->reasoning, add + 1);
            }
        }

        /* Tool calls from provider (Anthropic sends complete tool_use blocks,
         * OpenAI sends deltas — handled below) */
        if (delta->tool_calls_count > 0) {
            for (int i = 0; i < delta->tool_calls_count && i < 64; i++) {
                int idx = ctx->max_tc_idx;
                if (idx < 64) {
                    snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s",
                             delta->tool_calls[i].id);
                    snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s",
                             delta->tool_calls[i].name);
                    snprintf(ctx->tc_args[idx], sizeof(ctx->tc_args[idx]), "%s",
                             delta->tool_calls[i].arguments);
                    ctx->max_tc_idx = idx + 1;
                }
            }
        }

        ctx->prov_ops->free_response(delta);
    }

    /* Fallback: also try OpenAI-style delta parsing for tool calls.
     * This handles the case where parse_stream_chunk returns empty
     * but the chunk has tool_call deltas (OpenAI format).
     * HTTP layer already strips "data: " prefix — handle both cases. */
    const char *json_str = data;
    if (strncmp(data, "data: ", 6) == 0)
        json_str = data + 6;
    if (strncmp(json_str, "[DONE]", 6) == 0) {
            ctx->streaming_done = true;
            return 0;
        }

    char *err = NULL;
    json_node_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 0; }

    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *delta = json_object_get(choice, "delta");

        /* OpenAI tool call deltas */
        if (delta) {
            json_node_t *tc_delta = json_object_get(delta, "tool_calls");
            if (tc_delta && json_array_count(tc_delta) > 0) {
                for (size_t i = 0; i < json_array_count(tc_delta); i++) {
                    json_node_t *tc = json_array_get(tc_delta, i);
                    int idx = (int)json_object_get_number(tc, "index", 0);
                    if (idx >= 64) continue;
                    if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;
                    const char *id = json_object_get_string(tc, "id", NULL);
                    if (id && id[0])
                        snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", NULL);
                        if (name && name[0])
                            snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);
                        const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                        if (args_chunk && args_chunk[0]) {
                            size_t cur = strlen(ctx->tc_args[idx]);
                            size_t add = strlen(args_chunk);
                            if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                                memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                                ctx->tc_args[idx][cur + add] = '\0';
                            }
                        }
                    }
                }
            }

            /* OpenAI finish reason */
            const char *finish = json_object_get_string(choice, "finish_reason", NULL);
            if (finish && finish[0]) {
                ctx->streaming_done = true;
                snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);
                json_node_t *usage = json_object_get(root, "usage");
                if (usage) {
                    ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
                    ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
                }
            }
        }
    }

    json_free(root);
    return 0;
}


int on_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(data, &err);
    if (!root) { free(err); return 0; } /* Skip non-JSON events */

    json_node_t *choices = json_object_get(root, "choices");
    if (!choices || json_array_count(choices) == 0) { json_free(root); return 0; }

    json_node_t *choice = json_array_get(choices, 0);
    json_node_t *delta = json_object_get(choice, "delta");
    if (!delta) { json_free(root); return 0; }

    /* Extract finish_reason */
    const char *finish = json_object_get_string(choice, "finish_reason", NULL);
    if (finish && finish[0])
        snprintf(ctx->resp->finish_reason, sizeof(ctx->resp->finish_reason), "%s", finish);

    /* Extract delta content */
    const char *content = json_object_get_string(delta, "content", NULL);
    if (content && content[0]) {
        /* P95: Track first token timing */
        if (!ctx->first_token_flag) {
            ctx->first_token_flag = true;
            ctx->resp->diag.first_token_time = clock_mono_sec();
            ctx->resp->diag.first_token_received = true;
            ctx->resp->diag.time_to_first_token =
                ctx->resp->diag.first_token_time - ctx->req_start_time;
        }
        ctx->token_count++;
        /* Append to accumulated content */
        size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
        size_t add = strlen(content);
        char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
        if (newc) {
            ctx->resp->content = newc;
            memcpy(ctx->resp->content + cur, content, add + 1);
        }
        /* Call token callback */
        if (ctx->token_cb) {
            int cb_ret = ctx->token_cb(content, ctx->userdata);
            if (cb_ret != 0) {
                /* Caller requested abort (e.g. user interrupt) */
                json_free(root);
                return 1;
            }
        }
    }

    /* Extract delta tool_calls (streaming builds them across chunks) */
    json_node_t *tc_delta = json_object_get(delta, "tool_calls");
    if (tc_delta && json_array_count(tc_delta) > 0) {
        for (size_t i = 0; i < json_array_count(tc_delta); i++) {
            json_node_t *tc = json_array_get(tc_delta, i);
            int idx = (int)json_object_get_number(tc, "index", 0);
            if (idx >= 64) continue;

            /* Track highest index */
            if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;

            /* id (only present in first chunk for this tool_call) */
            const char *id = json_object_get_string(tc, "id", NULL);
            if (id && id[0]) snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);

            /* function delta */
            json_node_t *fn = json_object_get(tc, "function");
            if (fn) {
                const char *name = json_object_get_string(fn, "name", NULL);
                if (name && name[0]) snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);

                const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                if (args_chunk && args_chunk[0]) {
                    size_t cur = strlen(ctx->tc_args[idx]);
                    size_t add = strlen(args_chunk);
                    if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                        memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                        ctx->tc_args[idx][cur + add] = '\0';
                    }
                }
            }
        }
    }

    /* Extract reasoning (some providers send it in chunks too) */
    const char *reasoning = json_object_get_string(delta, "reasoning", NULL);
    if (!reasoning) reasoning = json_object_get_string(delta, "reasoning_content", NULL);
    if (reasoning && reasoning[0]) {
        size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
        size_t add = strlen(reasoning);
        char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
        if (newr) {
            ctx->resp->reasoning = newr;
            memcpy(ctx->resp->reasoning + cur, reasoning, add + 1);
        }
    }

    /* L07: xAI encrypted reasoning content in streaming delta */
    if (!ctx->resp->encrypted_content) {
        json_node_t *enc_content = json_object_get(delta, "encrypted_content");
        if (enc_content && json_array_count(enc_content) > 0) {
            char *ser = json_serialize(enc_content);
            if (ser) ctx->resp->encrypted_content = ser;
        }
    }

    /* Extract usage if present (final chunk with finish_reason) */
    if (finish && finish[0]) {
        json_node_t *usage = json_object_get(root, "usage");
        if (usage) {
            ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
            ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
        }
    }

    json_free(root);
    return 0;
}

void finalize_stream_toolcalls(stream_ctx_t *ctx) {
    ctx->resp->tool_calls_count = ctx->max_tc_idx;
    for (int i = 0; i < ctx->max_tc_idx; i++) {
        snprintf(ctx->resp->tool_calls[i].id, sizeof(ctx->resp->tool_calls[i].id), "%s", ctx->tc_id[i]);
        snprintf(ctx->resp->tool_calls[i].name, sizeof(ctx->resp->tool_calls[i].name), "%s", ctx->tc_name[i]);
        snprintf(ctx->resp->tool_calls[i].arguments, sizeof(ctx->resp->tool_calls[i].arguments), "%s", ctx->tc_args[i]);
    }
}

void populate_stream_diag_headers(llm_response_t *resp, const char *raw_headers) {
    if (!resp || !raw_headers) return;
    char buf[384] = {0};
    size_t pos = 0;

    /* Header names to capture (lowercase for case-insensitive search) */
    const char *headers_to_capture[] = {
        "cf-ray",
        "x-openrouter-provider",
        "x-openrouter-model",
        "x-openrouter-id",
        "x-request-id",
        "x-vercel-id",
        "server",
        NULL
    };

    for (int i = 0; headers_to_capture[i] && pos < sizeof(buf) - 64; i++) {
        const char *name = headers_to_capture[i];
        size_t nlen = strlen(name);

        /* Case-insensitive search in raw headers */
        const char *h = raw_headers;
        while ((h = strcasestr(h, name)) != NULL) {
            const char *val = h + nlen;
            while (*val == ':' || *val == ' ') val++;
            const char *end = val;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - val);
            if (vlen > 120) vlen = 120;
            if (vlen > 0) {
                size_t remaining = sizeof(buf) - pos;
                int written = snprintf(buf + pos, remaining, "%s=%.*s ",
                                       name, (int)vlen, val);
                if (written > 0) pos += (size_t)written;
            }
            h = end; /* Continue after this match */
            break;    /* Only first occurrence */
        }
    }

    if (pos > 0) {
        /* Remove trailing space */
        if (pos > 0 && buf[pos-1] == ' ') buf[pos-1] = '\0';
        strncpy(resp->diag.upstream_headers, buf, sizeof(resp->diag.upstream_headers) - 1);
        resp->diag.upstream_headers[sizeof(resp->diag.upstream_headers) - 1] = '\0';
    }
}

time_t extract_api_error_context(const http_resp_t *resp) {
    if (!resp || !resp->headers) return 0;

    time_t now = time(NULL);
    time_t reset_at = 0;

    /* 1. Retry-After header (RFC 7231) — seconds or HTTP-date */
    const char *h = resp->headers;
    const char *ra = strcasestr(h, "retry-after");
    if (ra) {
        const char *val = ra + 11;  /* len("retry-after") */
        while (*val == ':' || *val == ' ') val++;
        const char *end = val;
        while (*end && *end != '\r' && *end != '\n') end++;
        size_t vlen = (size_t)(end - val);
        if (vlen > 0 && vlen < 64) {
            char valbuf[64];
            memcpy(valbuf, val, vlen);
            valbuf[vlen] = '\0';
            double seconds = http_parse_retry_after(valbuf);
            if (seconds > 0) reset_at = now + (time_t)seconds;
        }
    }

    /* 2. X-RateLimit-Reset header */
    h = resp->headers;
    const char *rlr = strcasestr(h, "x-ratelimit-reset");
    if (rlr && reset_at == 0) {
        const char *val = rlr + 18;  /* len("x-ratelimit-reset") */
        while (*val == ':' || *val == ' ') val++;
        const char *end = val;
        while (*end && *end != '\r' && *end != '\n') end++;
        size_t vlen = (size_t)(end - val);
        if (vlen > 0 && vlen < 64) {
            char valbuf[64];
            memcpy(valbuf, val, vlen);
            valbuf[vlen] = '\0';
            char *ep = NULL;
            double ts = strtod(valbuf, &ep);
            if (ep != valbuf && *ep == '\0' && ts > 0) {
                if (ts > 1000000000.0)  /* Looks like Unix timestamp */
                    reset_at = (time_t)ts;
                else
                    reset_at = now + (time_t)ts;
            }
        }
    }

    /* 3. JSON body fields (retry_after, reset_at, resets_at) */
    if (resp->body && *resp->body && reset_at == 0) {
        json_t *body = json_parse(resp->body, NULL);
        if (body && body->type == JSON_OBJECT) {
            /* retry_after — seconds from now */
            const char *ra_val = json_get_str(body, "retry_after", NULL);
            if (ra_val) {
                char *ep = NULL;
                double seconds = strtod(ra_val, &ep);
                if (ep != ra_val && seconds > 0) reset_at = now + (time_t)seconds;
            }
            /* reset_at — Unix timestamp */
            if (reset_at == 0) {
                const char *rst = json_get_str(body, "reset_at", NULL);
                if (!rst) rst = json_get_str(body, "resets_at", NULL);
                if (rst) {
                    char *ep = NULL;
                    double ts = strtod(rst, &ep);
                    if (ep != rst && ts > 0) reset_at = (time_t)ts;
                }
            }
        }
        json_free(body);
    }

    return reset_at;
}

void finalize_stream_diag(stream_ctx_t *ctx) {
    if (!ctx->resp) return;
    stream_diag_t *d = &ctx->resp->diag;
    d->stream_end_time = clock_mono_sec();
    d->total_tokens = ctx->token_count;
    if (d->first_token_received) {
        d->total_stream_time = d->stream_end_time - d->first_token_time;
        if (d->total_stream_time > 0.001)
            d->tokens_per_second = (double)d->total_tokens / d->total_stream_time;
    }
}
