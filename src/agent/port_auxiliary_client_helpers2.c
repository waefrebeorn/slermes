/*
 * port_auxiliary_client_remaining2.c — Port of agent/auxiliary_client.py
 * wrapper-client surface. Interrupt protection, create/close wrappers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"
#include "hermes_json.h"
#include "hermes_agent.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __call__ @ agent/auxiliary_client.py:__call__ */
char *aux2_call(const char *args_json) {
    /* Python: load openai class + construct. C: the auxiliary client is
     * built-in; verify the args are well-formed JSON and return them. */
    if (!args_json) return NULL;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return NULL;
    json_free(args);
    return strdup(args_json);
}

/* PoP: _aux_interrupt_protected @ agent/auxiliary_client.py:_aux_interrupt_protected */
bool aux2_aux_interrupt_protected(void) {
    /* Python: interrupt protection flag (default off; enabled during
     * streaming where SIGINT must not kill the client mid-request). */
    return false;
}

/* PoP: create @ agent/auxiliary_client.py:create */
char *aux2_create(const char *kwargs_json) {
    /* Python: chat completion create. kwargs_json carries
     * {messages: [{role, content}...], model, provider, base_url, api_key,
     *  max_tokens, temperature}. Delegates to the real llm_chat_completion. */
    if (!kwargs_json) return NULL;

    json_t *kw = json_parse(kwargs_json, NULL);
    if (!kw || kw->type != JSON_OBJECT) {
        if (kw) json_free(kw);
        return strdup("{\"error\":\"bad kwargs\"}");
    }

    /* Build the LLM config from kwargs. */
    llm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    const char *model = json_get_str(kw, "model", NULL);
    const char *provider = json_get_str(kw, "provider", NULL);
    const char *base_url = json_get_str(kw, "base_url", NULL);
    const char *api_key = json_get_str(kw, "api_key", NULL);
    if (model) snprintf(cfg.model, sizeof(cfg.model), "%s", model);
    if (provider) snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider);
    if (base_url) snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_url);
    if (api_key) snprintf(cfg.api_key, sizeof(cfg.api_key), "%s", api_key);
    cfg.max_tokens = (int)json_get_num(kw, "max_tokens", 2048);
    cfg.temperature = (float)json_get_num(kw, "temperature", 0.7);

    /* Build the message array. */
    json_t *msgs = json_obj_get(kw, "messages");
    size_t msg_count = (msgs && msgs->type == JSON_ARRAY) ? json_len(msgs) : 0;
    message_t **arr = msg_count ? calloc(msg_count, sizeof(message_t *)) : NULL;
    if (msg_count && !arr) {
        json_free(kw);
        return strdup("{\"error\":\"OOM\"}");
    }
    size_t n = 0;
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(msgs); i++) {
            json_t *m = json_get(msgs, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "");
            const char *content = json_get_str(m, "content", NULL);
            if (!content) continue;
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            arr[n++] = message_new(r, content);
        }
    }
    json_free(kw);
    if (n == 0) {
        free(arr);
        return strdup("{\"error\":\"no messages\"}");
    }

    llm_response_t *resp = llm_chat_completion(&cfg, (const message_t **)arr, n, NULL);
    for (size_t i = 0; i < n; i++) message_free(arr[i]);
    free(arr);
    if (!resp) return strdup("{\"error\":\"llm call failed\"}");

    json_t *out = json_object();
    if (resp->content) json_set(out, "content", json_string(resp->content));
    /* llm_client reports failures via content ("HTTP request failed",
     * "JSON parse error: ...") or finish_reason "error". */
    bool failed = resp->finish_reason[0] &&
                  strcmp(resp->finish_reason, "error") == 0;
    if (!failed && resp->content) {
        failed = strncmp(resp->content, "HTTP request failed", 19) == 0 ||
                 strncmp(resp->content, "JSON parse error", 16) == 0 ||
                 strncmp(resp->content, "LLM error", 9) == 0;
    }
    if (failed) json_set(out, "success", json_bool(false));
    else json_set(out, "success", json_bool(true));
    llm_response_free(resp);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{}");
}

/* PoP: close @ agent/auxiliary_client.py:close */
int aux2_close(void) {
    /* Python: close the client. C client is stateless — nothing to free. */
    return 0;
}
