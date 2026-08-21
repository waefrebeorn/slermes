#define _POSIX_C_SOURCE 200809L
/* plugin_capabilities_pure.c — Faithful C11 port of the pure, deterministic
 * capability-declaration + consent-state logic in
 * hermes_cli/plugin_capabilities.py (plugin capability declarations + consent,
 * #64228).
 *
 * Ports (PoP):
 *   VALID_CAPABILITY_IDS membership  -> pc_is_valid_capability
 *   parse_declared_capabilities      -> pc_parse_declared_capabilities
 *
 *   capability_set_hash              -> pc_capability_set_hash (sha256 over sorted set)
 *   granted_capabilities             -> pc_granted_capabilities
 *   _legacy_gate_set                 -> pc_legacy_gate_set
 *   plugin_capability_granted        -> pc_plugin_capability_granted
 *
 * Config is passed as a JSON string (the full config or just the entry). IO /
 * logging / config load-save are NOT ported (not pure logic). All returned
 * strings are malloc'd (free with free()).
 *
 * Reuses libcrypto (crypto_sha256 + crypto_hex_encode) for the consent hash,
 * exactly as the Python uses hashlib.sha256.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* VALID_CAPABILITY_IDS — the canonical registry (1:1 with enforcing gates). */
static const char *PC_VALID_IDS[] = {
    "tools.override",
    "llm.provider_override",
    "llm.model_override",
    "llm.agent_id_override",
    "llm.profile_override",
    "llm.task_override",
    "gateway.platform_actions",
    NULL,
};

/* legacy_path per capability: list of config keys to walk. */
static const char *PC_LEGACY_PATHS[][3] = {
    {"allow_tool_override", NULL, NULL},                 /* tools.override */
    {"llm", "allow_provider_override", NULL},            /* llm.provider_override */
    {"llm", "allow_model_override", NULL},              /* llm.model_override */
    {"llm", "allow_agent_id_override", NULL},           /* llm.agent_id_override */
    {"llm", "allow_profile_override", NULL},            /* llm.profile_override */
    {"llm", "allow_task_override", NULL},               /* llm.task_override */
    {"allow_platform_actions", NULL, NULL},             /* gateway.platform_actions */
};

static int pc_idx(const char *id) {
    for (int i = 0; PC_VALID_IDS[i]; i++)
        if (strcmp(PC_VALID_IDS[i], id) == 0) return i;
    return -1;
}
/* PoP: pc_is_valid_capability @ hermes_cli/plugin_capabilities.py:VALID_CAPABILITY_IDS */

int pc_is_valid_capability(const char *id) {
    return id && pc_idx(id) >= 0;
}

/* Walk entry JSON following a legacy_path; return truthy bool. */
static int pc_legacy_gate_in_entry(json_t *entry, int spec_idx) {
    if (!entry || entry->type != JSON_OBJECT) return 0;
    json_t *node = entry;
    for (int i = 0; PC_LEGACY_PATHS[spec_idx][i]; i++) {
        if (!node || node->type != JSON_OBJECT) return 0;
        node = json_object_get(node, PC_LEGACY_PATHS[spec_idx][i]);
    }
    if (!node || node->type != JSON_BOOL) return 0;
    return node->bool_val ? 1 : 0;
}

/* Extract plugins.entries.<plugin_id> from a full config JSON string. */
static json_t *pc_entry_from_config(const char *config_json, const char *plugin_id) {
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return NULL; }
    json_t *plugins = json_object_get(cfg, "plugins");
    if (!plugins || plugins->type != JSON_OBJECT) { json_free(cfg); return NULL; }
    json_t *entries = json_object_get(plugins, "entries");
    if (!entries || entries->type != JSON_OBJECT) { json_free(cfg); return NULL; }
    json_t *entry = json_object_get(entries, plugin_id);
    if (!entry || entry->type != JSON_OBJECT) { json_free(cfg); return NULL; }
    /* detach so free(cfg) doesn't free entry */
    json_t *detached = json_copy(entry);
    json_free(cfg);
    return detached;
}
/* PoP: pc_parse_declared_capabilities @ hermes_cli/plugin_capabilities.py:parse_declared_capabilities */

char *pc_parse_declared_capabilities(const char *raw_json, const char *plugin_name) {
    json_t *raw = json_parse(raw_json, NULL);
    char *out = (char *)malloc(2);
    out[0] = '['; out[1] = '\0';
    size_t outlen = 1;
    if (raw && raw->type == JSON_ARRAY) {
        size_t n = json_len(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_array_get(raw, i);
            if (!item || item->type != JSON_STRING) continue;
            const char *cap = item->str_val;
            if (!cap || !*cap) continue;
            /* strip */
            char *s = strdup(cap); char *q = s;
            while (*q==' '||*q=='\t') q++;
            size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t')) q[--L]=0;
            if (!pc_is_valid_capability(q)) { free(s); continue; }
            /* dedupe */
            int dup = 0;
            for (size_t j = 0; j < i && !dup; j++) {
                json_t *e2 = json_array_get(raw, j);
                if (e2 && e2->type == JSON_STRING && pc_is_valid_capability(e2->str_val) &&
                    strcmp(e2->str_val, q) == 0) dup = 1;
            }
            if (dup) { free(s); continue; }
            char *esc = (json_serialize(json_string(q)));
            size_t need = outlen + (outlen>1?1:0) + strlen(esc?esc:"") + 1;
            out = (char *)realloc(out, need);
            strcat(out, (outlen>1)?",":"");
            strcat(out, esc ? esc : "\"\"");
            outlen += (outlen>1?1:0) + strlen(esc?esc:"");
            free(esc); free(s);
        }
    }
    json_free(raw);
    out = (char *)realloc(out, outlen + 2);
    strcat(out, "]");
    return out;
}

char *pc_capability_set_hash(const char *caps_json) {
    json_t *caps = json_parse(caps_json, NULL);
    /* collect valid ids, sort, join with \n, sha256 */
    const char *ids[16];
    int n = 0;
    if (caps && caps->type == JSON_ARRAY) {
        size_t m = json_len(caps);
        for (size_t i = 0; i < m && n < 16; i++) {
            json_t *e = json_array_get(caps, i);
            if (e && e->type == JSON_STRING && pc_is_valid_capability(e->str_val))
                ids[n++] = e->str_val;
        }
    }
    /* insertion sort by strcmp */
    for (int i = 1; i < n; i++) {
        const char *key = ids[i]; int j = i - 1;
        while (j >= 0 && strcmp(ids[j], key) > 0) { ids[j+1] = ids[j]; j--; }
        ids[j+1] = key;
    }
    /* build canon string with \n joins (dedup adjacent equal after sort) */
    char *canon = (char *)malloc(1); canon[0]='\0'; size_t cl=0;
    for (int i = 0; i < n; i++) {
        if (i > 0 && strcmp(ids[i], ids[i-1]) == 0) continue; /* dedup */
        size_t add = strlen(ids[i]) + 1;
        canon = (char *)realloc(canon, cl + add + 1);
        if (cl) strcat(canon, "\n");
        strcat(canon, ids[i]);
        cl += add;
    }
    unsigned char digest[32];
    crypto_sha256((const unsigned char *)canon, cl, digest);
    char *hex = crypto_hex_encode(digest, 32);
    free(canon);
    json_free(caps);
    return hex; /* malloc'd */
}
/* PoP: pc_granted_capabilities @ hermes_cli/plugin_capabilities.py:granted_capabilities */

char *pc_granted_capabilities(const char *plugin_id, const char *config_json) {
    json_t *entry = pc_entry_from_config(config_json, plugin_id);
    char *out = (char *)malloc(2);
    out[0] = '['; out[1] = '\0';
    size_t outlen = 1;
    if (entry) {
        json_t *raw = json_object_get(entry, "granted_capabilities");
        if (raw && raw->type == JSON_ARRAY) {
            size_t n = json_len(raw);
            for (size_t i = 0; i < n; i++) {
                json_t *c = json_array_get(raw, i);
                if (!c || c->type != JSON_STRING) continue;
                char *s = strdup(c->str_val); char *q = s;
                while (*q==' '||*q=='\t') q++;
                size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t')) q[--L]=0;
                if (!pc_is_valid_capability(q)) { free(s); continue; }
                /* dedupe */
                int dup = 0;
                for (size_t j = 0; j < i && !dup; j++) {
                    json_t *e2 = json_array_get(raw, j);
                    if (e2 && e2->type == JSON_STRING && pc_is_valid_capability(e2->str_val) &&
                        strcmp(e2->str_val, q) == 0) dup = 1;
                }
                if (dup) { free(s); continue; }
                char *esc = (json_serialize(json_string(q)));
                size_t need = outlen + (outlen>1?1:0) + strlen(esc?esc:"") + 1;
                out = (char *)realloc(out, need);
                strcat(out, (outlen>1)?",":"");
                strcat(out, esc ? esc : "\"\"");
                outlen += (outlen>1?1:0) + strlen(esc?esc:"");
                free(esc); free(s);
            }
        }
        json_free(entry);
    }
    out = (char *)realloc(out, outlen + 2);
    strcat(out, "]");
    return out;
}
/* PoP: pc_legacy_gate_set @ hermes_cli/plugin_capabilities.py:_legacy_gate_set */

int pc_legacy_gate_set(const char *entry_json, const char *capability) {
    int idx = pc_idx(capability);
    if (idx < 0) return 0;
    json_t *entry = json_parse(entry_json, NULL);
    int r = pc_legacy_gate_in_entry(entry, idx);
    if (entry) json_free(entry);
    return r;
}
/* PoP: pc_plugin_capability_granted @ hermes_cli/plugin_capabilities.py:plugin_capability_granted */

int pc_plugin_capability_granted(const char *plugin_id, const char *capability,
                                  const char *config_json) {
    int idx = pc_idx(capability);
    if (idx < 0) return 0; /* unknown id -> fail closed */
    json_t *config = json_parse(config_json, NULL);
    if (!config) return 0; /* fail closed on unparseable config */

    /* granted_capabilities check */
    json_t *entry = pc_entry_from_config(config_json, plugin_id);
    int granted = 0;
    if (entry) {
        json_t *raw = json_object_get(entry, "granted_capabilities");
        if (raw && raw->type == JSON_ARRAY) {
            size_t n = json_len(raw);
            for (size_t i = 0; i < n && !granted; i++) {
                json_t *c = json_array_get(raw, i);
                if (c && c->type == JSON_STRING && pc_is_valid_capability(c->str_val) &&
                    strcmp(c->str_val, capability) == 0) granted = 1;
            }
        }
        json_free(entry);
    }
    if (granted) { json_free(config); return 1; }
    /* legacy gate check */
    json_t *plugins = json_object_get(config, "plugins");
    json_t *entries = plugins ? json_object_get(plugins, "entries") : NULL;
    json_t *e2 = entries ? json_object_get(entries, plugin_id) : NULL;
    int legacy = pc_legacy_gate_in_entry(e2, idx);
    json_free(config);
    return legacy;
}
