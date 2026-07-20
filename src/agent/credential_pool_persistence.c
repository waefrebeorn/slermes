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
