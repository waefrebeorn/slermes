#define _POSIX_C_SOURCE 200809L
/* plugin_capabilities_pure.c - Faithful C11 port of the pure, deterministic
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* VALID_CAPABILITY_IDS - the canonical registry (1:1 with enforcing gates). */
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

/* PoP: pc_capability_set_hash @ hermes_cli/plugin_capabilities.py:capability_set_hash */
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
    if (!config) return 0; /* fail closed on unparseable config  */
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

/* ── Consent-state helpers ──────────────────────────────────────────────── */

/* Filter a JSON array of capability ids to valid ones, deduping while
 * preserving first-seen order (Python: dict.fromkeys). Returns a new array. */
static json_t *pc_valid_dedupe(const json_t *caps) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (caps && caps->type == JSON_ARRAY) {
        for (size_t i = 0; i < caps->c.count; i++) {
            json_t *e = caps->c.items[i];
            if (!e || e->type != JSON_STRING || !e->str_val) continue;
            char *s = strdup(e->str_val);
            if (!s) continue;
            char *q = s;
            while (*q==' '||*q=='\t') q++;
            size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t')) q[--L]=0;
            int dup = 0, valid = pc_is_valid_capability(q);
            if (!valid) { free(s); continue; }
            for (size_t j = 0; j < out->c.count && !dup; j++) {
                json_t *x = out->c.items[j];
                if (x->type == JSON_STRING && x->str_val && strcmp(x->str_val, q) == 0)
                    dup = 1;
            }
            if (!dup) json_append(out, json_string(q));
            free(s);
        }
    }
    return out;
}

/* PoP: pc_consent_hash @ hermes_cli/plugin_capabilities.py:consent_hash */
/* Return the stored consent hash for plugin_id (malloc'd), or NULL when
 * absent/corrupt. config_json may be the full config or just the entry. */
char *pc_consent_hash(const char *plugin_id, const char *config_json) {
    json_t *entry = pc_entry_from_config(config_json, plugin_id);
    if (!entry) return NULL;
    json_t *consent = json_obj_get(entry, "consent");
    char *out = NULL;
    if (consent && consent->type == JSON_OBJECT) {
        json_t *h = json_obj_get(consent, "hash");
        if (h && h->type == JSON_STRING && h->str_val && *h->str_val)
            out = strdup(h->str_val);
    }
    json_free(entry);
    return out;
}

/* PoP: pc_pending_capabilities @ hermes_cli/plugin_capabilities.py:pending_capabilities */
/* Capabilities declared by the plugin but not yet granted — everything is
 * pending at first consent; on update re-consent only the additions.
 * Returns a malloc'd JSON array string. */
char *pc_pending_capabilities(const char *plugin_id, const char *declared_json,
                              const char *config_json) {
    json_t *declared = json_parse(declared_json, NULL);
    json_t *filtered = pc_valid_dedupe(declared);
    if (declared) json_free(declared);
    char *granted_json = pc_granted_capabilities(plugin_id, config_json);
    json_t *granted = granted_json ? json_parse(granted_json, NULL) : NULL;
    free(granted_json);

    char *out = malloc(2);
    out[0] = '['; out[1] = '\0';
    size_t outlen = 1;
    if (filtered) {
        for (size_t i = 0; i < filtered->c.count; i++) {
            json_t *c = filtered->c.items[i];
            if (!c || c->type != JSON_STRING) continue;
            int in_granted = 0;
            if (granted && granted->type == JSON_ARRAY) {
                for (size_t j = 0; j < granted->c.count && !in_granted; j++) {
                    json_t *g = granted->c.items[j];
                    if (g && g->type == JSON_STRING && g->str_val &&
                        strcmp(g->str_val, c->str_val) == 0) in_granted = 1;
                }
            }
            if (in_granted) continue;
            char *esc = json_escape_string(c->str_val ? c->str_val : "");
            out = realloc(out, outlen + (outlen > 1 ? 1 : 0) + strlen(esc ? esc : "\"\"") + 1);
            strcat(out, (outlen > 1) ? "," : "");
            strcat(out, esc ? esc : "\"\"");
            outlen += (outlen > 1 ? 1 : 0) + strlen(esc ? esc : "");
            free(esc);
        }
        json_free(filtered);
    }
    if (granted) json_free(granted);
    out = realloc(out, outlen + 2);
    strcat(out, "]");
    return out;
}

/* PoP: pc_declared_set_changed @ hermes_cli/plugin_capabilities.py:declared_set_changed */
/* True when the declared set differs from what the user consented to. No
 * stored consent at all counts as changed (never consented). */
int pc_declared_set_changed(const char *plugin_id, const char *declared_json,
                            const char *config_json) {
    char *stored = pc_consent_hash(plugin_id, config_json);
    if (!stored) return 1;
    json_t *declared = json_parse(declared_json, NULL);
    json_t *filtered = pc_valid_dedupe(declared);
    if (declared) json_free(declared);
    char *ser = filtered ? json_serialize(filtered) : NULL;
    if (filtered) json_free(filtered);
    char *now_hash = ser ? pc_capability_set_hash(ser) : pc_capability_set_hash("[]");
    free(ser);
    int changed = !now_hash || strcmp(stored, now_hash) != 0;
    free(now_hash);
    free(stored);
    return changed;
}

/* PoP: pc_record_consent @ hermes_cli/plugin_capabilities.py:record_consent */
/* Pure core of record_consent: applies the consent decision to config_json
 * and returns the updated full-config JSON (malloc'd) — merging granted
 * capabilities with any previous grant (sorted, valid ids only), writing the
 * consent record (sha256 of the declared set + UTC timestamp), and mirroring
 * each grant into its legacy allow_* gate so existing enforcement keeps
 * working. Config load/save stays at the caller boundary. Returns NULL when
 * config_json doesn't parse as an object. */
char *pc_record_consent(const char *config_json, const char *plugin_id,
                        const char *granted_json, const char *declared_json)
{
    json_t *config = json_parse(config_json, NULL);
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return NULL; }

    json_t *granted_in = json_parse(granted_json, NULL);
    json_t *declared_in = json_parse(declared_json, NULL);
    json_t *granted_list = pc_valid_dedupe(granted_in);
    json_t *declared_list = pc_valid_dedupe(declared_in);
    if (granted_in) json_free(granted_in);
    if (declared_in) json_free(declared_in);

    /* plugins.entries.<plugin_id>, creating each level as needed. */
    json_t *plugins = json_obj_get(config, "plugins");
    if (!plugins || plugins->type != JSON_OBJECT) {
        plugins = json_object();
        json_set(config, "plugins", plugins);
    }
    json_t *entries = json_obj_get(plugins, "entries");
    if (!entries || entries->type != JSON_OBJECT) {
        entries = json_object();
        json_set(plugins, "entries", entries);
    }
    json_t *entry = json_obj_get(entries, plugin_id);
    if (!entry || entry->type != JSON_OBJECT) {
        entry = json_object();
        json_set(entries, plugin_id, entry);
    }

    /* Merge previously-granted capabilities into the new set. */
    json_t *merged = json_array();
    json_t *previous = json_obj_get(entry, "granted_capabilities");
    if (previous && previous->type == JSON_ARRAY) {
        for (size_t i = 0; i < previous->c.count; i++) {
            json_t *c = previous->c.items[i];
            if (c && c->type == JSON_STRING && c->str_val && pc_is_valid_capability(c->str_val)) {
                int dup = 0;
                for (size_t j = 0; j < merged->c.count && !dup; j++) {
                    json_t *x = merged->c.items[j];
                    if (strcmp(x->str_val, c->str_val) == 0) dup = 1;
                }
                if (!dup) json_append(merged, json_string(c->str_val));
            }
        }
    }
    for (size_t i = 0; i < granted_list->c.count; i++) {
        json_t *c = granted_list->c.items[i];
        int dup = 0;
        for (size_t j = 0; j < merged->c.count && !dup; j++) {
            json_t *x = merged->c.items[j];
            if (strcmp(x->str_val, c->str_val) == 0) dup = 1;
        }
        if (!dup) json_append(merged, json_copy(c));
    }
    /* Sort the stored set (Python: sorted(...)). */
    for (size_t i = 1; i < merged->c.count; i++) {
        json_t *keyv = merged->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(merged->c.items[j-1]->str_val, keyv->str_val) > 0) {
            merged->c.items[j] = merged->c.items[j-1];
            j--;
        }
        merged->c.items[j] = keyv;
    }
    json_set(entry, "granted_capabilities", merged);

    /* Consent record: hash of the declared set + UTC timestamp (seconds). */
    char *declared_ser = json_serialize(declared_list);
    char *hash = pc_capability_set_hash(declared_ser ? declared_ser : "[]");
    free(declared_ser);
    json_t *consent = json_object();
    if (hash) json_set(consent, "hash", json_string(hash));
    free(hash);
    char tsbuf[32];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%S+00:00", &tm_utc);
    json_set(consent, "granted_at", json_string(tsbuf));
    json_set(entry, "consent", consent);

    /* Bridge: mirror each granted capability into its legacy allow_* gate. */
    for (int gi = 0; PC_VALID_IDS[gi]; gi++) {
        int present = 0;
        for (size_t i = 0; i < merged->c.count && !present; i++)
            if (strcmp(merged->c.items[i]->str_val, PC_VALID_IDS[gi]) == 0) present = 1;
        if (!present) continue;
        json_t *node = entry;
        for (int k = 0; k < 3; k++) {
            const char *part = PC_LEGACY_PATHS[gi][k];
            if (!part) break;
            if (PC_LEGACY_PATHS[gi][k+1]) { /* intermediate: dict per level */
                json_t *child = json_obj_get(node, part);
                if (!child || child->type != JSON_OBJECT) {
                    child = json_object();
                    json_set(node, part, child);
                }
                node = child;
            } else {
                json_set(node, part, json_bool(true));
            }
        }
    }

    json_free(granted_list);
    json_free(declared_list);
    char *out = json_serialize(config);
    json_free(config);
    return out;
}
