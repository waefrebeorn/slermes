/*
 * provider.c -- real C11 port of the pure half of upstream Hermes
 *   plugins/browser/browser_use/provider.py (dual-auth config resolution,
 *   request shaping, the managed idempotency-key map and the create_session
 *   record).
 *
 * PoP: w20_provider_2f6167_resolve_config @ plugins/browser/browser_use/provider.py:_get_config_or_none
 * PoP: w20_provider_2f6167_resolve_config @ plugins/browser/browser_use/provider.py:_get_config
 * PoP: w20_provider_2f6167_headers @ plugins/browser/browser_use/provider.py:_headers
 * PoP: w20_provider_2f6167_release_body @ plugins/browser/browser_use/provider.py:_release_body
 * PoP: w20_provider_2f6167_create_payload @ plugins/browser/browser_use/provider.py:create_session
 * PoP: w20_provider_2f6167_should_preserve_pending_create_key @ plugins/browser/browser_use/provider.py:_should_preserve_pending_create_key
 * PoP: w20_provider_2f6167_pending_key_ops @ plugins/browser/browser_use/provider.py:_get_or_create_pending_create_key
 * PoP: w20_provider_2f6167_pending_key_ops @ plugins/browser/browser_use/provider.py:_clear_pending_create_key
 * PoP: w20_provider_2f6167_create_session @ plugins/browser/browser_use/provider.py:create_session
 *
 * The transport stays upstream: _post_create / _check_created belong to
 * plugins/browser/_common.py CloudBrowserProvider and need a live HTTP client,
 * so the caller hands in the scripted response instead.
 */
#include "hermes_json.h"
#include "noc_w20_provider_2f6167.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PR6167_BASE_URL "https://api.browser-use.com/api/v3"
#define PR6167_MANAGED_TIMEOUT_MINUTES 5
#define PR6167_MANAGED_PROXY_COUNTRY_CODE "us"
#define PR6167_KEY_PREFIX "browser-use-session-create:"
#define PR6167_MAP_SLOTS 32

static char *pr6167_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int pr6167_truthy_str(const char *s) { return s && s[0]; }

/* Python truthiness for the decoded scalars this module reads. */
static int pr6167_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static json_t *pr6167_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* --- _should_preserve_pending_create_key ---------------------------------- */

static int pr6167_contains_ci(const char *hay, const char *needle) {
    size_t n, i, j;
    if (!hay || !needle) return 0;
    n = strlen(needle);
    if (!n) return 1;
    for (i = 0; hay[i]; i++) {
        for (j = 0; j < n; j++) {
            char a = hay[i + j];
            if (!a) break;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (a != needle[j]) break;
        }
        if (j == n) return 1;
    }
    return 0;
}

int w20_provider_2f6167_should_preserve_pending_create_key(int status_code,
                                                           const json_t *payload) {
    const json_t *err;
    const char *message;
    if (status_code >= 500) return 1;
    if (status_code != 409) return 0;
    if (!payload || payload->type != JSON_OBJECT) return 0;   /* payload.get on a non-dict */
    err = json_obj_get(payload, "error");
    if (!err || err->type != JSON_OBJECT) return 0;
    if (!pr6167_truthy(json_obj_get(err, "message"))) return 0;  /* message or "" */
    message = json_get_str(err, "message", "");
    return pr6167_contains_ci(message, "already in progress");
}

/* --- _get_config_or_none / _get_config ------------------------------------- */

static json_t *pr6167_direct_config(const char *api_key) {
    json_t *cfg = json_object();
    json_set(cfg, "api_key", json_string(api_key));
    json_set(cfg, "base_url", json_string(PR6167_BASE_URL));
    json_set(cfg, "managed_mode", json_bool(0));
    return cfg;
}

static json_t *pr6167_managed_config(const json_t *seams) {
    const char *origin = json_get_str(seams, "managed_origin", "");
    const json_t *token = json_obj_get(seams, "managed_token");
    json_t *cfg;
    size_t n;
    char *trimmed;
    if (!json_get_bool(seams, "managed_available", 0)) return NULL;
    n = strlen(origin);
    while (n > 0 && origin[n - 1] == '/') n--;
    trimmed = (char *)malloc(n + 1);
    if (!trimmed) return NULL;
    memcpy(trimmed, origin, n);
    trimmed[n] = '\0';
    cfg = json_object();
    json_set(cfg, "api_key", (token && token->type != JSON_NULL) ? json_copy(token) : json_null());
    json_set(cfg, "base_url", json_string(trimmed));
    json_set(cfg, "managed_mode", json_bool(1));
    free(trimmed);
    return cfg;
}

/* read_selection("browser"): NULL when it returns None. */
static const char *pr6167_selected(const json_t *seams) {
    const json_t *v = json_obj_get(seams, "selected");
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

static json_t *pr6167_config_or_none(const json_t *seams) {
    const char *api_key = json_get_str(seams, "api_key", "");
    const char *selected = pr6167_selected(seams);
    if (selected && !strcmp(selected, "nous")) return pr6167_managed_config(seams);
    if (selected) return pr6167_truthy_str(api_key) ? pr6167_direct_config(api_key) : NULL;
    if (pr6167_truthy_str(api_key)) return pr6167_direct_config(api_key);
    return pr6167_managed_config(seams);
}

static char *pr6167_selection_error(const char *selection_name, const char *failure) {
    const char *head = "browser is configured to use ";
    const char *mid = " (set via hermes tools), but ";
    const char *tail = ". Run 'hermes tools' to change it.";
    size_t len = strlen(head) + strlen(selection_name) + strlen(mid) + strlen(failure) +
                  strlen(tail) + 1;
    char *out = (char *)malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s%s%s%s%s", head, selection_name, mid, failure, tail);
    return out;
}

json_t *w20_provider_2f6167_resolve_config(const json_t *seams) {
    json_t *out = json_object();
    json_t *cfg;
    const char *selected;
    if (!seams || seams->type != JSON_OBJECT) return out;   /* no seams: unreadable */
    cfg = pr6167_config_or_none(seams);
    if (cfg) {
        json_set(out, "config", cfg);
        json_set(out, "error", json_null());
        return out;
    }
    selected = pr6167_selected(seams);
    json_set(out, "config", json_null());
    if (selected && !strcmp(selected, "nous")) {
        char *msg = pr6167_selection_error(
            "nous", "the Nous Tool Gateway is not available (not entitled or unreachable)");
        json_set(out, "error", msg ? json_string(msg) : json_null());
        free(msg);
    } else if (selected) {
        char *msg = pr6167_selection_error(selected, "BROWSER_USE_API_KEY is not set");
        json_set(out, "error", msg ? json_string(msg) : json_null());
        free(msg);
    } else if (json_get_bool(seams, "tools_enabled", 0)) {
        json_set(out, "error", json_string(
            "Browser Use requires either a direct BROWSER_USE_API_KEY credential or a managed "
            "Browser Use gateway configuration."));
    } else {
        json_set(out, "error", json_string(
            "Browser Use requires a direct BROWSER_USE_API_KEY credential."));
    }
    return out;
}

/* --- request shaping ------------------------------------------------------- */

json_t *w20_provider_2f6167_headers(const json_t *config) {
    json_t *out = json_object();
    const json_t *key = json_obj_get(config, "api_key");
    json_set(out, "Content-Type", json_string("application/json"));
    json_set(out, "X-Browser-Use-API-Key", key ? json_copy(key) : json_null());
    return out;
}

json_t *w20_provider_2f6167_release_body(void) {
    json_t *out = json_object();
    json_set(out, "action", json_string("stop"));
    return out;
}

json_t *w20_provider_2f6167_create_payload(int managed_mode) {
    json_t *out = json_object();
    if (managed_mode) {
        /* Billing authorization must not default to a long Browser-Use timeout. */
        json_set(out, "timeout", json_number(PR6167_MANAGED_TIMEOUT_MINUTES));
        json_set(out, "proxyCountryCode", json_string(PR6167_MANAGED_PROXY_COUNTRY_CODE));
    }
    return out;
}

/* --- the managed idempotency-key map --------------------------------------- */

typedef struct {
    char *task_id;
    char *key;
} pr6167_slot;

static pr6167_slot PR6167_MAP[PR6167_MAP_SLOTS];
static unsigned long pr6167_key_counter = 0;

static pr6167_slot *pr6167_slot_find(const char *task_id) {
    int i;
    if (!task_id) return NULL;
    for (i = 0; i < PR6167_MAP_SLOTS; i++) {
        if (PR6167_MAP[i].task_id && !strcmp(PR6167_MAP[i].task_id, task_id)) return &PR6167_MAP[i];
    }
    return NULL;
}

static char *pr6167_new_key(void) {
    char buf[64];
    pr6167_key_counter++;
    snprintf(buf, sizeof(buf), PR6167_KEY_PREFIX "%032lx", pr6167_key_counter);
    return pr6167_dup(buf);
}

/* Python: `existing = map.get(task_id); if existing: return existing`. */
static const char *pr6167_get_or_create(const char *task_id, int *reused) {
    pr6167_slot *slot = pr6167_slot_find(task_id);
    int i;
    if (slot && slot->key && slot->key[0]) {
        if (reused) *reused = 1;
        return slot->key;
    }
    if (!slot) {
        for (i = 0; i < PR6167_MAP_SLOTS; i++) {
            if (!PR6167_MAP[i].task_id) {
                PR6167_MAP[i].task_id = pr6167_dup(task_id ? task_id : "");
                slot = &PR6167_MAP[i];
                break;
            }
        }
    }
    if (!slot) {
        if (reused) *reused = 0;
        return "";
    }
    free(slot->key);
    slot->key = pr6167_new_key();
    if (reused) *reused = 0;
    return slot->key ? slot->key : "";
}

static void pr6167_clear(const char *task_id) {
    pr6167_slot *slot = pr6167_slot_find(task_id);
    if (!slot) return;
    free(slot->key);
    slot->key = NULL;
}

json_t *w20_provider_2f6167_pending_key_ops(const json_t *ops) {
    json_t *out = json_array();
    size_t i, n;
    if (!ops || ops->type != JSON_ARRAY) return out;
    n = ops->c.count;
    for (i = 0; i < n; i++) {
        json_t *op = ops->c.items[i];
        const char *name, *task_id;
        json_t *rec = json_object();
        if (!op || op->type != JSON_OBJECT) {
            json_set(rec, "kind", json_string("skip"));
            json_append(out, rec);
            continue;
        }
        name = json_get_str(op, "op", "");
        task_id = json_get_str(op, "task_id", "");
        if (!strcmp(name, "get")) {
            int reused = 0;
            const char *key = pr6167_get_or_create(task_id, &reused);
            size_t hex = strlen(key) > strlen(PR6167_KEY_PREFIX)
                             ? strlen(key) - strlen(PR6167_KEY_PREFIX)
                             : 0;
            json_set(rec, "kind", json_string("key"));
            json_set(rec, "reused", json_bool(reused));
            json_set(rec, "prefix", json_string(PR6167_KEY_PREFIX));
            json_set(rec, "hex_len", json_number((double)hex));
        } else if (!strcmp(name, "clear")) {
            pr6167_clear(task_id);
            json_set(rec, "kind", json_string("void"));
        } else {
            json_set(rec, "kind", json_string("skip"));
        }
        json_append(out, rec);
    }
    return out;
}

/* --- create_session -------------------------------------------------------- */

json_t *w20_provider_2f6167_create_session(const json_t *seams, const char *task_id,
                                           const json_t *response) {
    json_t *resolved, *cfg, *record, *value, *body, *headers, *session_data;
    const char *api_key;
    int managed;
    json_t *id, *cdp, *expires;

    if (!seams || seams->type != JSON_OBJECT) seams = json_object();
    resolved = w20_provider_2f6167_resolve_config(seams);
    cfg = json_obj_get(resolved, "config");
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *err = json_obj_get(resolved, "error");
        json_t *out = pr6167_raised("ValueError",
                                    (err && err->type == JSON_STRING) ? err->str_val : "");
        json_free(resolved);
        return out;
    }
    cfg = json_copy(cfg);
    json_free(resolved);

    managed = pr6167_truthy(json_obj_get(cfg, "managed_mode"));
    api_key = json_get_str(cfg, "api_key", "");
    (void)api_key;
    body = response ? json_obj_get(response, "body") : NULL;
    headers = response ? json_obj_get(response, "headers") : NULL;
    if (managed) {
        /* managed_mode mints/forwards the idempotency key for the POST */
        (void)pr6167_get_or_create(task_id ? task_id : "", NULL);
        if (body && !json_get_bool(response, "ok", 1)) {
            /* A 4xx retrying cannot fix drops the key before the raise. */
            if (!w20_provider_2f6167_should_preserve_pending_create_key(
                    (int)json_get_num(response, "status", 0), body)) {
                pr6167_clear(task_id ? task_id : "");
            }
        }
    }
    session_data = body;
    id = json_obj_get(session_data, "id");
    if (!id) {
        json_t *out = pr6167_raised("KeyError", "'id'");
        json_free(cfg);
        return out;
    }

    record = json_object();
    {
        size_t n = strlen(task_id ? task_id : "");
        char *prefix = (char *)malloc(strlen("hermes__") + n + 1);
        if (prefix) {
            snprintf(prefix, strlen("hermes__") + n + 1, "hermes_%s_", task_id ? task_id : "");
            json_set(record, "session_name_prefix", json_string(prefix));
            free(prefix);
        }
    }
    json_set(record, "bb_session_id", json_copy(id));
    cdp = json_obj_get(session_data, "cdpUrl");
    if (!pr6167_truthy(cdp)) cdp = json_obj_get(session_data, "connectUrl");
    if (!pr6167_truthy(cdp)) cdp = NULL;
    json_set(record, "cdp_url", cdp ? json_copy(cdp) : json_string(""));
    expires = json_obj_get(session_data, "timeoutAt");
    json_set(record, "expires_at", expires ? json_copy(expires) : json_null());
    {
        json_t *features = json_object();
        json_set(features, "browser_use", json_bool(1));
        json_set(record, "features", features);
    }
    {
        json_t *ext = NULL;
        if (managed && headers) {
            json_t *v = json_obj_get(headers, "x-external-call-id");
            if (v) ext = json_copy(v);
        }
        json_set(record, "external_call_id", ext ? ext : json_null());
    }
    if (managed) pr6167_clear(task_id ? task_id : "");

    value = json_object();
    json_set(value, "status", json_string("ok"));
    json_set(value, "value", record);
    json_free(cfg);
    return value;
}
