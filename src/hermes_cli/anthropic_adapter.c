/*
 * port_agent_anthropic_adapter.c — C port of agent/anthropic_adapter.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "credential_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Declared in src/cli/port_auth_na.c (no dedicated header). Resolves the
 * active HERMES_HOME auth.json path. */
extern char *auth_file_path(void);

/* Declared in src/agent/anthropic_adapter_ports.c. */
extern char *antr_sanitize_tool_id(const char *tool_id);

/* PoP: cli_agent_anthropic_adapter__resolve_positive_anthropic_max_tokens @ agent/anthropic_adapter.py:_get_anthropic_sdk */
const char* cli_agent_anthropic_adapter__get_anthropic_sdk(void) {
    hermes_log(LOG_DEBUG, "anthropic_adapter", "_get_anthropic_sdk: lazy import not available in C");
    return NULL;
}

/* PoP: cli_agent_anthropic_adapter__resolve_positive_anthropic_max_tokens @ agent/anthropic_adapter.py:_resolve_positive_anthropic_max_tokens */
int cli_agent_anthropic_adapter__resolve_positive_anthropic_max_tokens(double value) {
    if (value != value) {
        return -1;
    }
    if (value <= 0.0) {
        return -1;
    }
    int floored = (int)value;
    return (floored > 0) ? floored : -1;
}

/* PoP: cli_agent_anthropic_adapter__supports_adaptive_thinking @ agent/anthropic_adapter.py:_supports_adaptive_thinking */
int cli_agent_anthropic_adapter__supports_adaptive_thinking(const char *model) {
    if (!model) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    if (strstr(lower, "claude") == NULL) {
        return 0;
    }
    static const char *legacy[] = {
        "claude-3", "claude-opus-4-0", "claude-opus-4.0",
        "claude-sonnet-4-0", "claude-sonnet-4.0",
        "claude-opus-4-2025", "claude-sonnet-4-2025",
        "claude-opus-4-5", "claude-opus-4.5",
        "claude-sonnet-4-5", "claude-sonnet-4.5",
        "claude-haiku-4-5", "claude-haiku-4.5", NULL
    };
    for (int i = 0; legacy[i]; i++) {
        if (strstr(lower, legacy[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__supports_xhigh_effort @ agent/anthropic_adapter.py:_supports_xhigh_effort */
int cli_agent_anthropic_adapter__supports_xhigh_effort(const char *model) {
    if (!cli_agent_anthropic_adapter__supports_adaptive_thinking(model)) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    static const char *no_xhigh[] = {
        "claude-opus-4-6", "claude-opus-4.6",
        "claude-sonnet-4-6", "claude-sonnet-4.6", NULL
    };
    for (int i = 0; no_xhigh[i]; i++) {
        if (strstr(lower, no_xhigh[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__forbids_sampling_params @ agent/anthropic_adapter.py:_forbids_sampling_params */
int cli_agent_anthropic_adapter__forbids_sampling_params(const char *model) {
    if (!model) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    if (strstr(lower, "claude") == NULL) {
        return 0;
    }
    static const char *no_xhigh[] = {
        "claude-opus-4-6", "claude-opus-4.6",
        "claude-sonnet-4-6", "claude-sonnet-4.6", NULL
    };
    for (int i = 0; no_xhigh[i]; i++) {
        if (strstr(lower, no_xhigh[i]) != NULL) {
            return 0;
        }
    }
    static const char *legacy[] = {
        "claude-3", "claude-opus-4-0", "claude-opus-4.0",
        "claude-sonnet-4-0", "claude-sonnet-4.0",
        "claude-opus-4-2025", "claude-sonnet-4-2025",
        "claude-opus-4-5", "claude-opus-4.5",
        "claude-sonnet-4-5", "claude-sonnet-4.5",
        "claude-haiku-4-5", "claude-haiku-4.5", NULL
    };
    for (int i = 0; legacy[i]; i++) {
        if (strstr(lower, legacy[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__supports_fast_mode @ agent/anthropic_adapter.py:_supports_fast_mode */
int cli_agent_anthropic_adapter__supports_fast_mode(const char *model) {
    if (!model) {
        return 0;
    }
    return (strstr(model, "opus-4-6") != NULL || strstr(model, "opus-4.6") != NULL);
}

/* PoP: cli_agent_anthropic_adapter__to_plain_data @ agent/anthropic_adapter.py:_to_plain_data */
int cli_agent_anthropic_adapter__to_plain_data(const char *data, char *buf, size_t bufsize) {
    if (!data || !buf || bufsize == 0) {
        return -1;
    }
    /* Strip data: URL prefix if present, return base64 data */
    const char *prefix = "data:";
    if (strncmp(data, prefix, strlen(prefix)) == 0) {
        const char *comma = strchr(data, ',');
        if (comma) {
            strncpy(buf, comma + 1, bufsize - 1);
            buf[bufsize - 1] = '\0';
            return 0;
        }
    }
    strncpy(buf, data, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return 0;
}

/* PoP: cli_agent_anthropic_adapter__sanitize_replay_block @ agent/anthropic_adapter.py:_sanitize_replay_block */
int cli_agent_anthropic_adapter__sanitize_replay_block(const char *block_json, char *buf, size_t bufsize) {
    if (!block_json || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "anthropic_adapter", "_sanitize_replay_block: invalid args");
        return -1;
    }
    /* LIVE: whitelist per block type; return None (drop) for anything that
     * isn't a dict or carries an unusable payload. C contract: 0 = kept
     * (buf holds sanitized JSON), 2 = dropped (LIVE None). */
    json_t *b = json_parse(block_json, NULL);
    if (!b || b->type != JSON_OBJECT) {
        if (b) json_free(b);
        return 2;
    }
    const char *btype = json_get_str(b, "type", NULL);
    json_t *out = NULL;
    bool drop = false;

    if (btype && strcmp(btype, "text") == 0) {
        json_t *tv = json_obj_get(b, "text");
        const char *text = (tv && tv->type == JSON_STRING) ? tv->str_val : NULL;
        /* blank / non-string text drops the block (#69512) */
        if (!text) { drop = true; }
        else {
            bool blank = true;
            for (const char *p = text; *p; p++)
                if (!isspace((unsigned char)*p)) { blank = false; break; }
            if (blank) drop = true;
        }
        if (!drop) {
            out = json_object();
            json_set(out, "type", json_string("text"));
            json_set(out, "text", json_string(text));
            json_t *cits = json_obj_get(b, "citations");
            if (cits && cits->type == JSON_ARRAY && json_len(cits) > 0)
                json_set(out, "citations", json_copy(cits));
            json_t *cc = json_obj_get(b, "cache_control");
            if (cc && cc->type == JSON_OBJECT)
                json_set(out, "cache_control", json_copy(cc));
        }
    } else if (btype && strcmp(btype, "thinking") == 0) {
        out = json_object();
        json_set(out, "type", json_string("thinking"));
        json_t *th = json_obj_get(b, "thinking");
        json_set(out, "thinking", json_string(th && th->type == JSON_STRING ? th->str_val : ""));
        json_t *sig = json_obj_get(b, "signature");
        if (sig && sig->type == JSON_STRING && sig->str_val[0])
            json_set(out, "signature", json_string(sig->str_val));
    } else if (btype && strcmp(btype, "redacted_thinking") == 0) {
        json_t *data = json_obj_get(b, "data");
        if (!data) drop = true;
        else {
            out = json_object();
            json_set(out, "type", json_string("redacted_thinking"));
            json_set(out, "data", json_copy(data));
        }
    } else if (btype && strcmp(btype, "tool_use") == 0) {
        json_t *idv = json_obj_get(b, "id");
        char *tid = antr_sanitize_tool_id(idv && idv->type == JSON_STRING ? idv->str_val : "");
        out = json_object();
        json_set(out, "type", json_string("tool_use"));
        json_set(out, "id", json_string(tid ? tid : "tool_0"));
        free(tid);
        json_t *nm = json_obj_get(b, "name");
        json_set(out, "name", json_string(nm && nm->type == JSON_STRING ? nm->str_val : ""));
        json_t *input = json_obj_get(b, "input");
        json_set(out, "input", (input && input->type == JSON_OBJECT) ? json_copy(input) : json_object());
        json_t *cc = json_obj_get(b, "cache_control");
        if (cc && cc->type == JSON_OBJECT)
            json_set(out, "cache_control", json_copy(cc));
    } else if (btype && strcmp(btype, "image") == 0) {
        json_t *src = json_obj_get(b, "source");
        if (!src || src->type != JSON_OBJECT) drop = true;
        else {
            out = json_object();
            json_set(out, "type", json_string("image"));
            json_set(out, "source", json_copy(src));
        }
    } else {
        drop = true;
    }
    json_free(b);

    if (drop || !out) { if (out) json_free(out); return 2; }
    hermes_log(LOG_DEBUG, "anthropic_adapter", "_sanitize_replay_block: sanitized block");
    char *ser = json_serialize(out);
    json_free(out);
    if (!ser) return -1;
    snprintf(buf, bufsize, "%s", ser);
    free(ser);
    return 0;
}

/* PoP: cli_agent_anthropic_adapter_sanitize_anthropic_kwargs @ agent/anthropic_adapter.py:sanitize_anthropic_kwargs */
int cli_agent_anthropic_adapter_sanitize_anthropic_kwargs(const char *model, int forbid_sampling, char *buf, size_t bufsize) {
    if (!model || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "anthropic_adapter", "sanitize_anthropic_kwargs: invalid args");
        return -1;
    }
    snprintf(buf, bufsize, "{\"model\":\"%s\",\"forbid_sampling\":%d}", model, forbid_sampling);
    hermes_log(LOG_DEBUG, "anthropic_adapter", "sanitize_anthropic_kwargs: model=%s forbid=%d", model, forbid_sampling);
    return 0;
}

/* PoP: token @ agent/anthropic_adapter.py:_resolve_anthropic_pool_token */
/* Return the first available Anthropic OAuth token from credential_pool
 * (read-only: never clears expired entries or triggers a network refresh, so a
 * bare resolve never mutates ~/.hermes/auth.json). Faithful to the Python
 * resolver: load_pool("anthropic")._available_entries(clear_expired=False,
 * refresh=False), filter auth_type == "oauth", coerce access_token (may be an
 * explicit null in a partially-written entry) then strip; return the first
 * non-empty token. Returns a malloc'd token (caller frees) or NULL. */
char *cli_agent_anthropic_adapter__resolve_anthropic_pool_token(void) {
    char *path = auth_file_path();
    if (!path) return NULL;
    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    free(path);
    if (err) free(err);
    if (!doc) return NULL;

    char *result = NULL;
    json_t *pool = json_obj_get(doc, "credential_pool");
    if (pool && pool->type == JSON_OBJECT) {
        json_t *anthropic = json_obj_get(pool, "anthropic");
        if (anthropic && anthropic->type == JSON_ARRAY) {
            size_t n = json_len(anthropic);
            for (size_t i = 0; i < n && !result; i++) {
                json_t *entry = json_get(anthropic, i);
                if (!entry || entry->type != JSON_OBJECT) continue;
                const char *at = json_get_str(entry, "auth_type", "");
                if (strcmp(at, "oauth") != 0) continue;
                /* str(key or "") — coerce a persisted null before strip */
                const char *raw = json_get_str(entry, "access_token", "");
                char buf[8192];
                snprintf(buf, sizeof(buf), "%s", raw ? raw : "");
                /* strip */
                char *s = buf;
                while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                size_t L = strlen(s);
                while (L > 0 && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r')) { s[L-1]='\0'; L--; }
                if (*s) result = strdup(s);
            }
        }
    }
    json_free(doc);
    return result;
}
