/*
 * model_setup_flows_azure.c — C home for upstream Hermes hermes_cli/model_setup_flows_azure.py.
 * PoP: w26_modelsetup_cfaac9_mode_label @ hermes_cli/model_setup_flows_azure.py:_azure_mode_label
 * PoP: w26_modelsetup_cfaac9_current @ hermes_cli/model_setup_flows_azure.py:_azure_current
 * PoP: w26_modelsetup_cfaac9_pick_model @ hermes_cli/model_setup_flows_azure.py:_azure_pick_model
 * PoP: w26_modelsetup_cfaac9_transport @ hermes_cli/model_setup_flows_azure.py:_azure_detect_transport
 * PoP: w26_modelsetup_cfaac9_constants @ hermes_cli/model_setup_flows_azure.py:<module constants>
 *
 * The wizard walks endpoint, auth mode, credentials, transport and model.  Each
 * step is a decision over values this file owns; the prompts (``_ask``), the
 * ``.env`` read and the endpoint probe are seams whose answers arrive as fixed
 * inputs.  An empty answer means "take the default", which is the branch the
 * ported code has to get exactly right: the transport default comes from the
 * current api_mode, the picker's default from the current model or the first
 * discovered deployment.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_modelsetup_cfaac9.h"

static const char *const MODELSETUP_PROVIDER = "azure-foundry";
static const char *const MODELSETUP_API_KEY_ENV = "AZURE_FOUNDRY_API_KEY";
static const char *const MODELSETUP_CHAT_MODE = "chat_completions";
static const char *const MODELSETUP_MESSAGES_MODE = "anthropic_messages";
static const char *const MODELSETUP_HTTP_PREFIX = "http://";
static const char *const MODELSETUP_HTTPS_PREFIX = "https://";
static const int MODELSETUP_PICK_LIMIT = 30;

/* A string field, or NULL when absent / not a string. */
static const char *az_str(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

/* str(obj.get(key) or default): a non-empty string as-is, a non-zero number as
 * Python prints it, anything falsy as the default. */
static char *az_or_default(const json_t *obj, const char *key, const char *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    char buf[64];
    const char *src = def;
    char *out;
    if (v && v->type == JSON_STRING && v->str_val[0]) {
        src = v->str_val;
    } else if (v && v->type == JSON_NUMBER && v->num_val != 0.0) {
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        src = buf;
    }
    out = (char *)malloc(strlen(src) + 1);
    if (out) strcpy(out, src);
    return out;
}

json_t *w26_modelsetup_cfaac9_mode_label(const char *mode) {
    if (mode && !strcmp(mode, MODELSETUP_CHAT_MODE)) return json_string("OpenAI-style");
    return json_string("Anthropic-style");
}

json_t *w26_modelsetup_cfaac9_current(const json_t *state) {
    const char *env_key = az_str(state, "env_key");
    const json_t *config = state ? json_obj_get(state, "config") : NULL;
    const json_t *model_cfg = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "model") : NULL;
    json_t *out = json_object();
    char *base_url, *api_mode, *auth_mode;
    const json_t *entra = NULL;

    if (model_cfg && model_cfg->type == JSON_OBJECT && az_str(model_cfg, "provider") &&
        !strcmp(az_str(model_cfg, "provider"), MODELSETUP_PROVIDER)) {
        base_url = az_or_default(model_cfg, "base_url", "");
        api_mode = az_or_default(model_cfg, "api_mode", "");
        auth_mode = az_or_default(model_cfg, "auth_mode", "api_key");
        /* str(...).strip().lower() or "api_key" */
        {
            char *start = auth_mode, *end;
            size_t n;
            while (*start && isspace((unsigned char)*start)) start++;
            n = strlen(start);
            while (n > 0 && isspace((unsigned char)start[n - 1])) n--;
            memmove(auth_mode, start, n);
            auth_mode[n] = '\0';
            for (end = auth_mode; *end; end++) *end = (char)tolower((unsigned char)*end);
            if (!auth_mode[0]) {
                free(auth_mode);
                auth_mode = (char *)malloc(8);
                if (auth_mode) strcpy(auth_mode, "api_key");
            }
        }
        /* model_cfg.get("entra") or {} -- only a non-empty mapping survives */
        {
            const json_t *raw = json_obj_get(model_cfg, "entra");
            if (raw && raw->type == JSON_OBJECT && json_object_size(raw) > 0) entra = raw;
        }
    } else {
        base_url = az_or_default(NULL, "base_url", "");
        api_mode = az_or_default(NULL, "api_mode", "");
        auth_mode = az_or_default(NULL, "auth_mode", "api_key");
    }
    json_set(out, "base_url", json_string(base_url ? base_url : ""));
    json_set(out, "api_mode", json_string(api_mode ? api_mode : ""));
    json_set(out, "auth_mode", json_string(auth_mode ? auth_mode : "api_key"));
    json_set(out, "entra", entra ? json_copy(entra) : json_object());
    json_set(out, "api_key", json_string(env_key ? env_key : ""));
    free(base_url);
    free(api_mode);
    free(auth_mode);
    return out;
}

json_t *w26_modelsetup_cfaac9_pick_model(const json_t *state) {
    const json_t *models = state ? json_obj_get(state, "models") : NULL;
    const json_t *answer = state ? json_obj_get(state, "answer") : NULL;
    const char *current = az_str(state, "current");
    size_t count = (models && models->type == JSON_ARRAY) ? json_len(models) : 0;

    if (!answer || answer->type == JSON_NULL) return json_null();
    if (answer->type != JSON_STRING) return json_null();

    if (count == 0) {
        /* no deployments discovered: the typed name, else the current model */
        if (answer->str_val[0]) return json_string(answer->str_val);
        return json_string(current ? current : "");
    }
    if (!answer->str_val[0]) {
        const json_t *first = json_get(models, 0);
        if (current && current[0]) return json_string(current);
        if (first && first->type == JSON_STRING) return json_string(first->str_val);
        return json_null();
    }
    {
        const char *s = answer->str_val;
        size_t i, limit;
        int digits = 1;
        for (i = 0; s[i]; i++) {
            if (!isdigit((unsigned char)s[i])) { digits = 0; break; }
        }
        if (digits) {
            int pick = atoi(s);
            limit = count < (size_t)MODELSETUP_PICK_LIMIT ? count : (size_t)MODELSETUP_PICK_LIMIT;
            if (pick >= 1 && (size_t)pick <= limit) {
                const json_t *chosen = json_get(models, (size_t)(pick - 1));
                if (chosen && chosen->type == JSON_STRING) return json_string(chosen->str_val);
                return json_null();
            }
        }
    }
    return json_string(answer->str_val);
}

json_t *w26_modelsetup_cfaac9_transport(const json_t *state) {
    const json_t *detected = state ? json_obj_get(state, "detected") : NULL;
    const char *api_mode = az_str(detected, "api_mode");
    const json_t *models = detected ? json_obj_get(detected, "models") : NULL;
    const json_t *answer = state ? json_obj_get(state, "answer") : NULL;
    const char *current_mode = az_str(state, "current_api_mode");
    json_t *out;

    if (api_mode && api_mode[0]) {
        out = json_array();
        json_append(out, json_string(api_mode));
        json_append(out, models ? json_copy(models) : json_array());
        return out;
    }
    if (!answer || answer->type == JSON_NULL) return json_null();
    if (answer->type != JSON_STRING) return json_null();
    {
        const char *def = (current_mode && !strcmp(current_mode, MODELSETUP_MESSAGES_MODE)) ? "2" : "1";
        const char *chosen = answer->str_val[0] ? answer->str_val : def;
        out = json_array();
        json_append(out, json_string(strcmp(chosen, "2") == 0 ? MODELSETUP_MESSAGES_MODE : MODELSETUP_CHAT_MODE));
        json_append(out, models ? json_copy(models) : json_array());
        return out;
    }
}

json_t *w26_modelsetup_cfaac9_constants(void) {
    json_t *o = json_object();
    json_t *modes = json_array();
    json_t *auth = json_array();
    json_append(modes, json_string(MODELSETUP_CHAT_MODE));
    json_append(modes, json_string(MODELSETUP_MESSAGES_MODE));
    json_append(auth, json_string("api_key"));
    json_append(auth, json_string("entra_id"));
    json_set(o, "provider", json_string(MODELSETUP_PROVIDER));
    json_set(o, "api_key_env", json_string(MODELSETUP_API_KEY_ENV));
    json_set(o, "api_modes", modes);
    json_set(o, "auth_modes", auth);
    {
        json_t *http = json_array();
        json_append(http, json_string(MODELSETUP_HTTP_PREFIX));
        json_append(http, json_string(MODELSETUP_HTTPS_PREFIX));
        json_set(o, "http_prefix", http);
    }
    json_set(o, "pick_limit", json_number((double)MODELSETUP_PICK_LIMIT));
    return o;
}
