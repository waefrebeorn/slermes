#define _POSIX_C_SOURCE 200809L
/* plugin_packs_pure.c - Faithful C11 port of the pure, deterministic
 * validation helpers in hermes_cli/plugin_packs.py (declarative plugin packs).
 *
 * Ports (PoP):
 *   _EXACT_SHA_RE.fullmatch          -> plugin_pack_is_exact_sha
 *   _entry_label                      -> plugin_pack_entry_label
 *   validate_config_seed              -> plugin_pack_validate_config_seed
 *     (returns malloc'd JSON: the validated dict on success, or
 *      {"error":"..."} on rejection - exactly what the Python raises as
 *      PackError, so the oracle can diff it)
 *   secret/allow_/reserved key checks -> inlined inside validate_config_seed
 *
 * YAML parsing (parse_pack/load_pack) and index resolution/render are NOT
 * ported - they need the yaml lib + network + rich rendering.
 *
 * Rejects mirror Python: non-dict seed, non-str/empty keys, reserved keys
 * (granted_capabilities/capabilities_consent), allow_* trust gates, and
 * secret-shaped key names.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* _EXACT_SHA_RE = ^[0-9a-fA-F]{40}$  */
/* PoP: plugin_pack_is_exact_sha @ hermes_cli/plugin_packs.py:_EXACT_SHA_RE.fullmatch */

int plugin_pack_is_exact_sha(const char *ref) {
    if (!ref) return 0;
    size_t n = strlen(ref);
    if (n != 40) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = ref[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

/* _entry_label(item, index) -> "'name'" | "'repo'" | "#index+1"  */
/* PoP: plugin_pack_entry_label @ hermes_cli/plugin_packs.py:_entry_label */

char *plugin_pack_entry_label(json_t *item, int index) {
    const char *name = NULL, *repo = NULL;
    if (item && item->type == JSON_OBJECT) {
        json_t *n = json_object_get(item, "name");
        json_t *r = json_object_get(item, "repo");
        if (n && n->type == JSON_STRING) name = n->str_val;
        if (r && r->type == JSON_STRING) repo = r->str_val;
    }
    const char *base = (name && *name) ? name : repo;
    if (base && *base) {
        /* "'<base>'": base already stripped in caller? mirror Python .strip() */
        char *s = strdup(base);
        /* strip */
        char *p = s; while (*p == ' ' || *p == '\t') p++;
        size_t l = strlen(p); while (l > 0 && (p[l-1]==' '||p[l-1]=='\t')) { p[--l]=0; }
        char *out = (char *)malloc(l + 4);
        snprintf(out, l + 4, "'%s'", p);
        free(s);
        return out;
    }
    char *out = (char *)malloc(16);
    snprintf(out, 16, "#%d", index + 1);
    return out;
}

/* case-insensitive search for a secret-shaped token in a key */
static int key_is_secret_shaped(const char *key) {
    /* tokens: token, secret, password/passwd, api_key/api-key,
     * private_key/private-key, credential, auth */
    static const char *toks[] = {
        "token", "secret", "password", "passwd", "api_key", "api-key",
        "private_key", "private-key", "credential", "auth", NULL
    };
    char low[256];
    size_t l = strlen(key);
    if (l >= sizeof(low)) l = sizeof(low) - 1;
    for (size_t i = 0; i < l; i++) low[i] = (char)tolower((unsigned char)key[i]);
    low[l] = '\0';
    for (int t = 0; toks[t]; t++) {
        if (strstr(low, toks[t])) return 1;
    }
    return 0;
}

/* validate_config_seed(plugin_id, seed_json) -> malloc'd JSON string.
 * Success: serialized validated dict. Failure: {"error":"..."}.  */
/* PoP: plugin_pack_validate_config_seed @ hermes_cli/plugin_packs.py:validate_config_seed */

char *plugin_pack_validate_config_seed(const char *plugin_id, const char *seed_json) {
    char pid[256];
    snprintf(pid, sizeof(pid), "%s", plugin_id ? plugin_id : "");

    json_t *seed = json_parse(seed_json, NULL);
    if (!seed || seed->type != JSON_OBJECT) {
        char *out = (char *)malloc(512);
        snprintf(out, 512,
            "{\"error\":\"Pack config for plugin '%s' must be a mapping of plugins.entries.%s keys.\"}",
            pid, pid);
        if (seed) json_free(seed);
        return out;
    }

    /* Build validated copy, checking each key. */
    json_t *validated = json_object();
    /* NOTE: json_len() is array-only (returns 0 for objects); objects need
     * json_object_size(). The old code iterated 0 keys — every seed passed
     * validation unchecked. */
    size_t n = json_object_size(seed);
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(seed, i);
        json_t *val = json_object_get_at(seed, i);
        if (!key || !*key) {
            char *out = (char *)malloc(512);
            snprintf(out, 512,
                "{\"error\":\"Pack config for plugin '%s' has an invalid key: %s.\"}",
                pid, key ? key : "None");
            json_free(validated); json_free(seed);
            return out;
        }
        if (strcmp(key, "granted_capabilities") == 0 ||
            strcmp(key, "capabilities_consent") == 0 ||
            strncmp(key, "allow_", 6) == 0) {
            char *out = (char *)malloc(512);
            snprintf(out, 512,
                "{\"error\":\"Pack config for plugin '%s' sets reserved key '%s': "
                "packs cannot pre-grant capabilities or trust gates. Capability "
                "consent happens interactively at install time.\"}",
                pid, key);
            json_free(validated); json_free(seed);
            return out;
        }
        if (key_is_secret_shaped(key)) {
            char *out = (char *)malloc(512);
            snprintf(out, 512,
                "{\"error\":\"Pack config for plugin '%s' sets secret-shaped key '%s': "
                "secrets never travel in packs. Declare the secret in the plugin's "
                "requires_env instead — it is prompted at install.\"}",
                pid, key);
            json_free(validated); json_free(seed);
            return out;
        }
        /* deep-copy: json_object_set/json_set stores the node itself (takes
         * ownership), and json_free frees children — sharing val between
         * validated and seed would double-free on the success path. */
        json_t *val_copy = json_copy(val);
        json_object_set(validated, key, val_copy);
    }

    char *serialized = json_serialize(validated);
    if (!serialized) serialized = strdup("{}");
    json_free(validated);
    json_free(seed);
    return serialized;
}
