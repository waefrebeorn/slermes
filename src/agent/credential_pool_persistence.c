/* Port of Python agent/credential_pool.py (split module).

Self-contained credential-pool subsystem component. credential_pool_t /
credential_entry_t are defined in include/credential_pool.h; internal helpers
shared across the split modules are declared in include/credential_pool_internals.h.
No god headers — only the minimal includes each module requires. C11 only.
*/

#include "credential_pool.h"
#include "credential_pool_internals.h"
#include "credential_persistence.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/*
 * Component 3/4 — borrowed-credential sanitization (AG29):
 * is_borrowed_credential_source / sanitize_borrowed_credential_payload
 * + prune control.
 *
 * The key-normalization, secret-classification and SHA-256 fingerprint
 * primitives live in src/agent/credential_persistence.c and are reused via
 * credential_persistence.h — they are NOT duplicated here.
 */

/* Sources Hermes owns and can intentionally persist in auth.json */
static const char *PERSISTABLE_SOURCES[][2] = {
    {"anthropic", "hermes_pkce"},
    {"minimax-oauth", "oauth"},
    {"nous", "device_code"},
    {"openai-codex", "device_code"},
    {"xai-oauth", "loopback_pkce"},
    {NULL, NULL}
};

/* Check if a source is borrowed (not owned by Hermes) */
/* Port of Python agent/credential_persistence.py:is_borrowed_credential_source(). */
bool is_borrowed_credential_source(const char *source, const char *provider_id) {
    if (!source || !*source) return false;
    /* Manual entries are owned */
    if (strcasecmp(source, "manual") == 0 || strncasecmp(source, "manual:", 7) == 0)
        return false;
    /* Check persistable list */
    for (int i = 0; PERSISTABLE_SOURCES[i][0]; i++) {
        if (strcasecmp(provider_id, PERSISTABLE_SOURCES[i][0]) == 0 &&
            strcasecmp(source, PERSISTABLE_SOURCES[i][1]) == 0)
            return false;
    }
    return true;
}

/* Sanitize a credential payload for disk writing. Name parity: Python calls this
 * sanitize_borrowed_credential_payload(). */
/* Port of Python agent/credential_persistence.py:sanitize_borrowed_credential_payload(). */
json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* Get source field */
    json_t *source_node = json_obj_get(payload, "source");
    const char *source = source_node && source_node->type == JSON_STRING ? source_node->str_val : "";

    /* If not borrowed, return a copy as-is */
    if (!is_borrowed_credential_source(source, provider_id)) {
        char *serialized = json_serialize(payload);
        json_node_t *copy = json_parse(serialized, NULL);
        free(serialized);
        return copy;
    }

    /* Borrowed: strip secret keys, keep metadata + fingerprint */
    json_node_t *result = json_new_object();

    /* Copy non-secret fields (reuses shared is_secret_payload_key()). */
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *key = payload->c.keys[i];
        json_t *val = payload->c.items[i];
        if (!is_secret_payload_key(key)) {
            char *vstr = json_serialize(val);
            json_t *vcopy = json_parse(vstr, NULL);
            free(vstr);
            json_set(result, key, vcopy);
        }
    }

    /* Add fingerprint (reuses shared credential_secret_fingerprint()). */
    char *fp = credential_secret_fingerprint(payload);
    if (fp) {
        json_set(result, "secret_fingerprint", json_string(fp));
        free(fp);
    }

    return result;
}

/* Serialize one credential_entry_t to its disk-safe JSON object.
 * Port of Python agent/credential_pool.py:CredentialEntry.to_dict() — builds
 * the entry dict from the persisted fields, then runs
 * sanitize_borrowed_credential_payload() (exactly as Python does at
 * credential_pool.py:202). The resulting object is what gets written to
 * auth.json's providers.<provider> array. Caller frees. */
json_node_t *credential_entry_to_json(const credential_entry_t *e, const char *provider) {
    if (!e) return NULL;

    json_node_t *obj = json_new_object();

    /* Identity / metadata (non-secret). */
    if (e->label[0]) json_set(obj, "label", json_string(e->label));
    if (e->source[0]) json_set(obj, "source", json_string(e->source));
    if (e->scope[0]) json_set(obj, "scope", json_string(e->scope));
    if (e->base_url[0]) json_set(obj, "base_url", json_string(e->base_url));
    if (e->inference_base_url[0])
        json_set(obj, "inference_base_url", json_string(e->inference_base_url));
    if (e->agent_key_expires_at[0])
        json_set(obj, "agent_key_expires_at", json_string(e->agent_key_expires_at));
    if (e->expires_at_ms > 0)
        json_set(obj, "expires_at_ms", json_number((double)e->expires_at_ms));

    /* Secret-bearing fields — left intact here; sanitize() strips them for
     * borrowed sources and replaces with secret_fingerprint. */
    if (e->api_key[0]) json_set(obj, "api_key", json_string(e->api_key));
    if (e->access_token[0]) json_set(obj, "access_token", json_string(e->access_token));
    if (e->refresh_token[0]) json_set(obj, "refresh_token", json_string(e->refresh_token));
    if (e->agent_key[0]) json_set(obj, "agent_key", json_string(e->agent_key));

    /* Extra key/value bag (mirrors Python 'extra'). */
    if (e->extra && e->extra->type == JSON_OBJECT) {
        char *extra_ser = json_serialize(e->extra);
        if (extra_ser) {
            json_node_t *extra_copy = json_parse(extra_ser, NULL);
            free(extra_ser);
            if (extra_copy) json_set(obj, "extra", extra_copy);
        }
    }

    /* Run the disk-safety boundary (to_dict -> sanitize), as Python does. */
    json_node_t *sanitized = sanitize_borrowed_credential_payload(obj, provider);
    json_free(obj);
    return sanitized;
}

/* JSON-in entry point: build a credential_entry_t from a JSON object (the
 * shape the load path produces) and serialize it disk-safe. Reuses
 * credential_entry_to_json() — no duplicated logic. Caller frees. */
json_node_t *credential_entry_to_json_from_obj(const json_t *entry, const char *provider) {
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    credential_entry_t e;
    memset(&e, 0, sizeof(e));
    const json_t *v;
#define PICK_STR(k, dst) do { \
        v = json_obj_get(entry, k); \
        if (v && v->type == JSON_STRING && v->str_val[0]) \
            snprintf(e.dst, sizeof(e.dst), "%s", v->str_val); \
    } while (0)
    PICK_STR("label", label);
    PICK_STR("source", source);
    PICK_STR("scope", scope);
    PICK_STR("base_url", base_url);
    PICK_STR("inference_base_url", inference_base_url);
    PICK_STR("agent_key_expires_at", agent_key_expires_at);
    PICK_STR("api_key", api_key);
    PICK_STR("access_token", access_token);
    PICK_STR("refresh_token", refresh_token);
    PICK_STR("agent_key", agent_key);
#undef PICK_STR
    v = json_obj_get(entry, "expires_at_ms");
    if (v && v->type == JSON_NUMBER) e.expires_at_ms = (long long)v->num_val;
    v = json_obj_get(entry, "extra");
    if (v && v->type == JSON_OBJECT) {
        char *es = json_serialize(v);
        e.extra = es ? json_parse(es, NULL) : NULL;
        free(es);
    }
    json_node_t *out = credential_entry_to_json(&e, provider);
    if (e.extra) json_free(e.extra);
    return out;
}

/* Serialize all entries of a pool to the providers.<provider> entry-array
 * shape Python writes to auth.json ([entry.to_dict() for entry in entries]).
 * Returns a malloc'd JSON string (the providers entry ARRAY, not the root
 * object) or NULL. Caller frees. */
char *credential_pool_entries_json(const credential_pool_t *pool) {
    if (!pool) return NULL;
    json_node_t *arr = json_array();
    if (!arr) return NULL;
    for (int i = 0; i < pool->entry_count; i++) {
        json_node_t *entry = credential_entry_to_json(&pool->entries[i], pool->provider_name);
        if (entry) json_append(arr, entry);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser;
}
