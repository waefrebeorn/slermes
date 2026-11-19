/*
 * port_acp_client.c — Port of Python: agent/acp_client.py (JSON-RPC /
 * session-model shaping).
 *
 * Pure JSON transforms: message builders, the enabled-id list walk, the
 * config-option lookup, the model-id extraction for both the v1 and the legacy
 * ACP shapes, the selection request, and the wall-clock timeout normaliser.
 */

#include "port_acp_client.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *str_strip_dup(const char *s)
{
    if (!s) return dup_str("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python str(x or "") for a JSON node (None and falsey scalars -> ""). */
static char *node_str(const json_t *node)
{
    if (!node) return dup_str("");
    char buf[64];
    switch (node->type) {
    case JSON_NULL:   return dup_str("");
    case JSON_STRING: return dup_str(node->str_val ? node->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", node->num_val); return dup_str(buf);
    case JSON_BOOL:   return dup_str(node->bool_val ? "True" : "False");
    default:          return json_serialize(node);
    }
}

/* PoP: acp_jsonrpc_result @ agent/acp_client.py:_jsonrpc_result */
json_t *acp_jsonrpc_result(const char *id_json, const char *result_json)
{
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "id", id_json ? json_parse(id_json, NULL) : json_null());
    json_set(out, "result", result_json ? json_parse(result_json, NULL) : json_null());
    return out;
}

/* PoP: acp_jsonrpc_error @ agent/acp_client.py:_jsonrpc_error */
json_t *acp_jsonrpc_error(const char *id_json, int code, const char *message)
{
    json_t *out = json_object();
    if (!out) return NULL;
    json_t *err = json_object();
    json_set(err, "code", json_number((double)code));
    json_set(err, "message", json_string(message ? message : ""));
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "id", id_json ? json_parse(id_json, NULL) : json_null());
    json_set(out, "error", err);
    return out;
}

/* PoP: acp_enabled_id_list @ agent/acp_client.py:_enabled_id_list */
json_t *acp_enabled_id_list(const char *entries_json, const char *key)
{
    json_t *out = json_array();
    if (!out) return NULL;
    json_t *entries = entries_json ? json_parse(entries_json, NULL) : NULL;
    if (!entries || entries->type != JSON_ARRAY) {
        if (entries) json_free(entries);
        return out;
    }

    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        char *raw = node_str(json_obj_get(entry, key ? key : ""));
        char *value = str_strip_dup(raw ? raw : "");
        free(raw);
        if (!value || !*value) { free(value); continue; }

        int duplicate = 0;
        for (size_t j = 0; j < out->c.count; j++) {
            const char *seen = json_string_value(out->c.items[j]);
            if (seen && strcmp(seen, value) == 0) { duplicate = 1; break; }
        }
        if (duplicate) { free(value); continue; }
        json_append(out, json_string(value));
        free(value);
    }
    json_free(entries);
    return out;
}

/* PoP: acp_model_config_option @ agent/acp_client.py:_model_config_option */
json_t *acp_model_config_option(const char *session_json)
{
    json_t *session = session_json ? json_parse(session_json, NULL) : NULL;
    if (!session || session->type != JSON_OBJECT) {
        if (session) json_free(session);
        return NULL;
    }
    json_t *options = json_obj_get(session, "configOptions");
    if (!options || options->type != JSON_ARRAY) { json_free(session); return NULL; }

    json_t *found = NULL;
    for (size_t i = 0; i < options->c.count; i++) {
        json_t *option = options->c.items[i];
        if (!option || option->type != JSON_OBJECT) continue;
        const char *category = json_get_str(option, "category", NULL);
        const char *id = json_get_str(option, "id", NULL);
        if ((category && strcmp(category, "model") == 0) ||
            (id && strcmp(id, "model") == 0)) {
            found = json_copy(option);
            break;
        }
    }
    json_free(session);
    return found;
}

/* PoP: acp_legacy_session_model_ids @ agent/acp_client.py:_legacy_session_model_ids */
json_t *acp_legacy_session_model_ids(const char *session_json)
{
    json_t *session = session_json ? json_parse(session_json, NULL) : NULL;
    char *available = NULL;
    if (session && session->type == JSON_OBJECT) {
        json_t *models = json_obj_get(session, "models");
        if (models && models->type == JSON_OBJECT) {
            json_t *node = json_obj_get(models, "availableModels");
            if (node) available = json_serialize(node);
        }
    }
    json_t *out = acp_enabled_id_list(available, "modelId");
    free(available);
    if (session) json_free(session);
    return out;
}

/* PoP: acp_session_model_ids @ agent/acp_client.py:_session_model_ids */
json_t *acp_session_model_ids(const char *session_json)
{
    json_t *option = acp_model_config_option(session_json);
    if (option) {
        json_t *options = json_obj_get(option, "options");
        char *text = options ? json_serialize(options) : NULL;
        json_t *out = acp_enabled_id_list(text, "value");
        free(text);
        json_free(option);
        return out;
    }
    return acp_legacy_session_model_ids(session_json);
}

/* PoP: acp_model_selection_request @ agent/acp_client.py:_model_selection_request */
json_t *acp_model_selection_request(const char *session_json,
                                    const char *requested_model,
                                    const char *placeholder)
{
    json_t *session = session_json ? json_parse(session_json, NULL) : NULL;
    if (!session || session->type != JSON_OBJECT) {
        if (session) json_free(session);
        return NULL;
    }

    char *raw_session_id = node_str(json_obj_get(session, "sessionId"));
    char *session_id = str_strip_dup(raw_session_id ? raw_session_id : "");
    char *requested = str_strip_dup(requested_model ? requested_model : "");
    const char *ph = placeholder ? placeholder : "";

    json_t *out = NULL;
    if (!session_id || !*session_id || !requested || !*requested ||
        strcmp(requested, ph) == 0)
        goto done;

    json_t *option = acp_model_config_option(session_json);
    if (option) {
        json_t *ids = acp_session_model_ids(session_json);
        int allowed = 0;
        for (size_t i = 0; ids && i < ids->c.count; i++) {
            const char *v = json_string_value(ids->c.items[i]);
            if (v && strcmp(v, requested) == 0) { allowed = 1; break; }
        }
        if (ids) json_free(ids);
        if (!allowed) { json_free(option); goto done; }

        const char *config_id = json_get_str(option, "id", NULL);
        out = json_object();
        json_set(out, "method", json_string("session/set_config_option"));
        json_t *params = json_object();
        json_set(params, "sessionId", json_string(session_id));
        json_set(params, "configId",
                 json_string((config_id && *config_id) ? config_id : "model"));
        json_set(params, "value", json_string(requested));
        json_set(out, "params", params);
        json_free(option);
        goto done;
    }

    {
        json_t *available = acp_legacy_session_model_ids(session_json);
        int allowed = 1;
        if (available && available->c.count > 0) {
            allowed = 0;
            for (size_t i = 0; i < available->c.count; i++) {
                const char *v = json_string_value(available->c.items[i]);
                if (v && strcmp(v, requested) == 0) { allowed = 1; break; }
            }
        }
        if (available) json_free(available);
        if (!allowed) goto done;

        out = json_object();
        json_set(out, "method", json_string("session/set_model"));
        json_t *params = json_object();
        json_set(params, "sessionId", json_string(session_id));
        json_set(params, "modelId", json_string(requested));
        json_set(out, "params", params);
    }

done:
    free(session_id);
    free(requested);
    free(raw_session_id);
    json_free(session);
    return out;
}

/* PoP: acp_effective_timeout @ agent/acp_client.py:_effective_timeout */
double acp_effective_timeout(int is_number, double number,
                             const acp_timeout_t *obj)
{
    if (is_number) return number;
    if (!obj) return ACP_DEFAULT_TIMEOUT_SECONDS;

    double best = ACP_DEFAULT_TIMEOUT_SECONDS;
    int have = 0;
    const struct { int present; double value; } parts[] = {
        { obj->has_read, obj->read }, { obj->has_write, obj->write },
        { obj->has_connect, obj->connect }, { obj->has_pool, obj->pool },
        { obj->has_timeout, obj->timeout },
    };
    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        if (!parts[i].present) continue;
        if (!have || parts[i].value > best) best = parts[i].value;
        have = 1;
    }
    return have ? best : ACP_DEFAULT_TIMEOUT_SECONDS;
}
