/* Slermes C11 port — gateway/platforms/api_server.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: *_approval_event_choices @ gateway/platforms/api_server.py:_approval_event_choices */
json_t *_approval_event_choices(bool smart_denied, bool allow_permanent) {
    json_t *choices = json_array();
    if (smart_denied) {
        json_append(choices, json_string("once"));
        json_append(choices, json_string("deny"));
        return choices;
    }
    json_append(choices, json_string("once"));
    json_append(choices, json_string("session"));
    if (allow_permanent) json_append(choices, json_string("always"));
    json_append(choices, json_string("deny"));
    return choices;
}

/* PoP: *_clean_request_string @ gateway/platforms/api_server.py:_clean_request_string */
json_t *_clean_request_string(json_t *value) {
    if (!value || !json_is_string(value)) return NULL;
    const char *s = json_get_str(value, "", NULL);
    while (*s == ' ' || *s == '\t') s++;
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
    if (len == 0) return NULL;
    char *buf = (char*)malloc(len + 1);
    memcpy(buf, s, len); buf[len] = '\0';
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

/* PoP: *_request_reasoning_config @ gateway/platforms/api_server.py:_request_reasoning_config */
json_t *_request_reasoning_config(json_t *model_options) {
    if (!model_options || !json_is_object(model_options)) return NULL;
    json_t *reasoning = json_object_get(model_options, "reasoning");
    if (reasoning && json_is_object(reasoning)) return json_copy(reasoning);
    json_t *effort = json_object_get(model_options, "reasoning_effort");
    if (effort) {
        json_t *cfg = json_object();
        json_set(cfg, "effort", json_copy(effort));
        return cfg;
    }
    return NULL;
}

/* PoP: *_request_service_tier @ gateway/platforms/api_server.py:_request_service_tier */
json_t *_request_service_tier(json_t *model_options) {
    if (!model_options || !json_is_object(model_options))
        return json_string("_REQUEST_OPTION_MISSING");
    json_t *tier = json_object_get(model_options, "service_tier");
    if (!tier) return json_string("_REQUEST_OPTION_MISSING");
    return json_copy(tier);
}

/* PoP: *_apply_runtime_agent_overrides @ gateway/platforms/api_server.py:_apply_runtime_agent_overrides */
json_t *_apply_runtime_agent_overrides(json_t *runtime_kwargs, json_t *overrides) {
    if (!runtime_kwargs) runtime_kwargs = json_object();
    if (!overrides || !json_is_object(overrides)) return json_copy(runtime_kwargs);
    return json_copy(runtime_kwargs);
}

/* PoP: *_resolve_request_runtime_agent_kwargs @ gateway/platforms/api_server.py:_resolve_request_runtime_agent_kwargs */
json_t *_resolve_request_runtime_agent_kwargs(json_t *model_options) {
    if (!model_options || !json_is_object(model_options)) return json_object();
    json_t *agent = json_object_get(model_options, "agent");
    if (agent && json_is_object(agent)) return json_copy(agent);
    return json_object();
}

/* PoP: *_request_agent_overrides @ gateway/platforms/api_server.py:_request_agent_overrides */
json_t *_request_agent_overrides(json_t *model_options) {
    if (!model_options || !json_is_object(model_options)) return NULL;
    json_t *ov = json_object_get(model_options, "agent_overrides");
    if (ov && json_is_object(ov)) return json_copy(ov);
    return NULL;
}

/* PoP: *_message_text_prefix @ gateway/platforms/api_server.py:_message_text_prefix */
json_t *_message_text_prefix(json_t *msg) {
    if (!msg) return json_string("");
    const char *role = json_get_str(msg, "role", NULL);
    if (strcmp(role, "assistant") == 0) return json_string("Assistant: ");
    if (strcmp(role, "user") == 0) return json_string("User: ");
    if (strcmp(role, "system") == 0) return json_string("System: ");
    return json_string("");
}

/* PoP: _is_compressed_summary_message @ gateway/platforms/api_server.py:_is_compressed_summary_message */
bool _is_compressed_summary_message(json_t *msg) {
    if (!msg) return false;
    const char *type = json_get_str(msg, "type", NULL);
    return strcmp(type, "compressed_summary") == 0;
}

/* PoP: *_auto_truncate_response_history @ gateway/platforms/api_server.py:_auto_truncate_response_history */
json_t *_auto_truncate_response_history(json_t *history) {
    if (!history || !json_is_array(history)) return json_array();
    size_t n = json_len(history);
    if (n <= 10) return json_copy(history);
    json_t *t = json_array();
    for (size_t i = n - 10; i < n; i++)
        json_append(t, json_copy(json_get(history, i)));
    return t;
}

/* PoP: check_api_server_requirements @ gateway/platforms/api_server.py:check_api_server_requirements */
bool check_api_server_requirements(void) {
    return true;
}

/* PoP: *_redact_api_error_text @ gateway/platforms/api_server.py:_redact_api_error_text */
json_t *_redact_api_error_text(json_t *value, int limit) {
    if (!value) return json_string("");
    const char *s = json_get_str(value, "", NULL);
    if (limit > 0 && (int)strlen(s) > limit) {
        char *buf = (char*)malloc(limit + 1);
        memcpy(buf, s, limit); buf[limit] = '\0';
        json_t *r = json_string(buf);
        free(buf);
        return r;
    }
    return json_string(s);
}

/* PoP: *_openai_error @ gateway/platforms/api_server.py:_openai_error */
json_t *_openai_error(const char *message, const char *err_type, const char *param, const char *code) {
    json_t *inner = json_object();
    json_set(inner, "message", json_string(message ? message : ""));
    json_set(inner, "type", json_string(err_type ? err_type : "invalid_request_error"));
    if (param) json_set(inner, "param", json_string(param));
    if (code) json_set(inner, "code", json_string(code));
    json_t *err = json_object();
    json_set(err, "error", inner);
    return err;
}

/* PoP: _gateway_is_draining @ gateway/platforms/api_server.py:_gateway_is_draining */
bool _gateway_is_draining(void) {
    extern bool gateway_runner_is_draining(void);
    return gateway_runner_is_draining();
}

/* PoP: *_draining_response @ gateway/platforms/api_server.py:_draining_response */
json_t *_draining_response(void) {
    if (!_gateway_is_draining()) return NULL;
    return _openai_error("Gateway is draining existing work", "server_error", NULL, NULL);
}
