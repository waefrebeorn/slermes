/*
 * port_agent_gemini_cloudcode_adapter_methods.c — C port of
 * agent/gemini_cloudcode_adapter.py
 *
 * Gemini Cloud Code adapter concrete methods. Real HTTPS calls to the
 * Google Generative Language API (generateContent / streamGenerateContent).
 */

#include "hermes_logger.h"
#include "libhttp/http.h"
#include "libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GEMINI_API_BASE "https://generativelanguage.googleapis.com/v1beta/models"

/* Build a Gemini contents payload from an OpenAI-style messages JSON array.
 * Returns a malloc'd JSON string, or NULL on failure. */
static char *gemini_build_contents(const char *messages_json)
{
    json_t *msgs = json_parse(messages_json, NULL);
    if (!msgs || msgs->type != JSON_ARRAY) {
        if (msgs) json_free(msgs);
        return NULL;
    }

    /* Accumulate parts text from role=user / role=system messages. */
    char *contents = malloc(1);
    contents[0] = '\0';
    size_t cap = 1, len = 0;
    for (size_t i = 0; i < msgs->c.count; i++) {
        json_t *m = msgs->c.items[i];
        if (!m || m->type != JSON_OBJECT) continue;
        const char *role = json_get_str(m, "role", NULL);
        const char *content = json_get_str(m, "content", NULL);
        if (!content) {
            /* content may be an array of parts */
            json_t *carr = json_obj_get(m, "content");
            if (carr && carr->type == JSON_ARRAY && carr->c.count > 0)
                content = json_get_str(carr->c.items[0], "text", NULL);
        }
        if (!content) continue;
        const char *grole = (role && strcmp(role, "assistant") == 0) ? "model" : "user";

        char *esc = malloc(strlen(content) * 2 + 8);
        size_t e = 0;
        for (const char *p = content; *p; p++) {
            if (*p == '"' || *p == '\\') esc[e++] = '\\';
            esc[e++] = *p;
        }
        esc[e] = '\0';

        char part[4096];
        int n = snprintf(part, sizeof(part),
                         "{\"role\":\"%s\",\"parts\":[{\"text\":\"%s\"}]},", grole, esc);
        free(esc);
        if (n < 0) continue;
        size_t add = (size_t)n;
        if (len + add + 32 >= cap) { cap = len + add + 256; contents = realloc(contents, cap); }
        memcpy(contents + len, part, add);
        len += add;
    }
    if (msgs) json_free(msgs);

    /* Wrap: {"contents":[ ... ]} */
    char *out = malloc(len + 32);
    if (len > 0) len--; /* drop trailing comma */
    snprintf(out, len + 32, "{\"contents\":[%.*s]}", (int)len, contents);
    free(contents);
    return out;
}

/* PoP: gemini_cloudcode_adapter__create_chat_completion @ agent/gemini_cloudcode_adapter.py:_create_chat_completion */
/* Real Gemini generateContent call; translates response to OpenAI-format JSON. */
char *gemini_cloudcode_adapter__create_chat_completion(void *adapter, const char *model,
                                                         const char *messages, const char *tools)
{
    (void)tools;
    if (!adapter || !model || !messages) {
        hermes_log(LOG_ERROR, "gemini_adapter", "_create_chat_completion: invalid args");
        return NULL;
    }

    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key || !api_key[0]) {
        hermes_log(LOG_ERROR, "gemini_adapter", "GEMINI_API_KEY not set");
        return NULL;
    }

    char *contents = gemini_build_contents(messages);
    if (!contents) {
        hermes_log(LOG_ERROR, "gemini_adapter", "failed to build Gemini contents");
        return NULL;
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/%s:generateContent?key=%s", GEMINI_API_BASE, model, api_key);

    char *body = malloc(strlen(contents) + 64);
    snprintf(body, strlen(contents) + 64, "{\"contents\":%s}", contents + strlen("{\"contents\":"));

    http_t *http = http_new(60);
    char *result = NULL;
    if (http) {
        http_resp_t *res = http_post_json(http, url, body);
        if (res && res->status >= 200 && res->status < 300 && res->body) {
            json_t *doc = json_parse(res->body, NULL);
            if (doc && doc->type == JSON_OBJECT) {
                json_t *cands = json_obj_get(doc, "candidates");
                const char *text = "";
                if (cands && cands->type == JSON_ARRAY && cands->c.count > 0) {
                    json_t *c0 = cands->c.items[0];
                    json_t *content = c0 ? json_obj_get(c0, "content") : NULL;
                    json_t *parts = content ? json_obj_get(content, "parts") : NULL;
                    if (parts && parts->type == JSON_ARRAY && parts->c.count > 0)
                        text = json_get_str(parts->c.items[0], "text", "");
                }
                char *esc = malloc(strlen(text) * 2 + 8);
                size_t e = 0;
                for (const char *p = text; *p; p++) {
                    if (*p == '"' || *p == '\\') esc[e++] = '\\';
                    esc[e++] = *p;
                }
                esc[e] = '\0';
                result = malloc(strlen(esc) + 256);
                snprintf(result, strlen(esc) + 256,
                         "{\"id\":\"gemini-%s\",\"object\":\"chat.completion\","
                         "\"model\":\"%s\",\"choices\":[{\"message\":{\"role\":\"assistant\","
                         "\"content\":\"%s\"}}]}", model, model, esc);
                free(esc);
            }
            if (doc) json_free(doc);
        } else {
            hermes_log(LOG_ERROR, "gemini_adapter", "generateContent HTTP %d", res ? res->status : -1);
        }
        if (res) http_resp_free(res);
        http_free(http);
    }
    free(body);
    free(contents);
    return result;
}

/* PoP: gemini_cloudcode_adapter__stream_completion @ agent/gemini_cloudcode_adapter.py:_stream_completion */
/* Real Gemini streamGenerateContent (alt=sse) call; returns the raw SSE body,
 * which the caller projects into OpenAI chat.completion.chunk events. */
char *gemini_cloudcode_adapter__stream_completion(void *adapter, const char *model,
                                                    const char *messages, int stream)
{
    if (!adapter || !model || !messages) {
        hermes_log(LOG_ERROR, "gemini_adapter", "_stream_completion: invalid args");
        return NULL;
    }

    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key || !api_key[0]) {
        hermes_log(LOG_ERROR, "gemini_adapter", "GEMINI_API_KEY not set");
        return NULL;
    }

    char *contents = gemini_build_contents(messages);
    if (!contents) return NULL;

    char url[1024];
    snprintf(url, sizeof(url), "%s/%s:streamGenerateContent?alt=sse&key=%s",
             GEMINI_API_BASE, model, api_key);

    char *body = malloc(strlen(contents) + 64);
    snprintf(body, strlen(contents) + 64, "{\"contents\":%s}", contents + strlen("{\"contents\":"));

    http_t *http = http_new(60);
    char *result = NULL;
    if (http) {
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "Content-Type: application/json%s",
                 stream ? "" : "");
        http_resp_t *res = http_request(http, HTTP_POST, url, hdr, body, strlen(body));
        if (res && res->body) {
            result = strdup(res->body);
        } else {
            hermes_log(LOG_ERROR, "gemini_adapter", "streamGenerateContent HTTP %d", res ? res->status : -1);
        }
        if (res) http_resp_free(res);
        http_free(http);
    }
    free(body);
    free(contents);
    return result;
}
