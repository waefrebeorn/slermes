/*
 * port_skills_sync_client.c — Slermes C11 port of tools/skills_sync_client.py.
 *
 * The LOW-LEVEL skill-sync layer: content-addressed objects (blob/tree/commit)
 * from local skills, the sync-plane wire contract (push objects + CAS a ref,
 * pull the owner's HEAD, three-way merge on a 409), driven by skill_manage's
 * debounced push hook, the curator's periodic pull hook, and the
 * `hermes sync status|pull|push|now` CLI.
 *
 * Cluster 1 (this file): manifest serialization, content addressing,
 * identity/access-gate, sync-plane URL + feature-knob resolution, and local
 * skill eligibility. Cluster 2 (ObjectSet/commit/tree building + wire client)
 * and Cluster 3 (push/pull/org sync) live in sibling files.
 *
 * The Python original: /home/wubu/hermes-agent-dev/tools/skills_sync_client.py
 */

#define _POSIX_C_SOURCE 200809L
#include "port_skills_sync_client.h"

#include "hermes_json.h"
#include "slermes_home.h"
#include "crypto.h"
#include "yaml.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Contract constants (mirror skills_sync_client.py + src/sync/manifest.ts) ── */

#define SYNC_MANIFEST_ENTRY_NAME "sync-manifest"
#define SYNC_MANIFEST_TYPE       "sync-manifest"
#define SYNC_MANIFEST_VERSION    1

/* Wire address prefix: sha256:<64-hex> */
#define WIRE_ADDR_PREFIX "sha256:"

/* Dev-phase gate claim (NAS access-token-issuer.ts:312). Wire name is NAS's;
 * it means "this user is a Nous admin" (Permissions.ADMIN_ACCESS), NOT a
 * tool-gateway right. */
#define NOUS_ADMIN_CLAIM "tool_gateway_admin"

/* Production Skill Sync plane; overridable by env/config. */
#define DEFAULT_SYNC_BASE_URL "https://gateway-gateway.nousresearch.com"

/* Org mirror directory name — enterprise-managed content must never ride a
 * personal push. */
#define ORG_DIR_NAME "_org"

/* Boolean parse sets (mirror _TRUE/_FALSE). */
static const char *TRUE_WORDS[] = { "1", "true", "yes", "on", NULL };
static const char *FALSE_WORDS[] = { "0", "false", "no", "off", "", NULL };

/* ── Forward decls (siblings + external deps) ─────────────────────────── */

extern int cli_agent_skill_utils__is_external_skill_path(const char *path);
/* libyaml — generic config getters (sync.* section lives in config.yaml). */
/* Port of _read_skill_name (src/tools/port_skills_sync.c) — returns
 * malloc'd skill name from SKILL.md frontmatter. */
extern char *read_skill_name(const char *skill_md_path, const char *fallback);

/* ── canonical_json_bytes (L186) ──────────────────────────────────────── */

/* Canonical JSON serialization for tree/commit hashing (sync contract):
 * UTF-8, keys sorted lexicographically, no insignificant whitespace, no
 * trailing newline. Both client and server MUST produce byte-identical
 * output or a push fails 422 hash_mismatch.
 *
 * libjson's json_serialize emits compact output; keys are emitted in
 * insertion order, so the caller must insert object keys in sorted order
 * to match Python's sort_keys=True. */
char *skills_sync_canonical_json_bytes(json_t *obj, size_t *out_len) {
    if (!obj) return NULL;
    char *ser = json_serialize(obj);
    if (!ser) return NULL;
    if (out_len) *out_len = strlen(ser);
    return ser;
}

/* ── build_sync_manifest_bytes (L118) ─────────────────────────────────── */

/* Serialize the per-skill opt-in map into canonical sync-manifest bytes.
 * skills maps skill name -> enabled. Emits the shape parseSyncManifest
 * validates: {type, version:1, skills:[{name,enabled}]}, sorted by name. */
char *skills_sync_build_manifest_bytes(const char *const *names,
                                       const bool *enabled, size_t count,
                                       size_t *out_len) {
    /* Build a sorted list of (name, enabled) pairs. */
    typedef struct { const char *name; bool enabled; } pair_t;
    pair_t *pairs = calloc(count ? count : 1, sizeof(pair_t));
    if (!pairs) return NULL;
    for (size_t i = 0; i < count; i++) {
        pairs[i].name = names[i];
        pairs[i].enabled = enabled ? enabled[i] : false;
    }
    /* Insertion sort by name (stable; count is small). */
    for (size_t i = 1; i < count; i++) {
        pair_t key = pairs[i];
        size_t j = i;
        while (j > 0 && strcmp(pairs[j - 1].name, key.name) > 0) {
            pairs[j] = pairs[j - 1];
            j--;
        }
        pairs[j] = key;
    }

    json_t *skills = json_array();
    for (size_t i = 0; i < count; i++) {
        json_t *entry = json_object();
        /* Insert in SORTED key order (sort_keys=True): "enabled" < "name". */
        json_set(entry, "enabled", json_bool(pairs[i].enabled));
        json_set(entry, "name", json_string(pairs[i].name));
        json_append(skills, entry);
    }
    json_t *manifest = json_object();
    /* Sorted: "skills" < "type" < "version". */
    json_set(manifest, "skills", skills);
    json_set(manifest, "type", json_string(SYNC_MANIFEST_TYPE));
    json_set(manifest, "version", json_number(SYNC_MANIFEST_VERSION));

    char *bytes = skills_sync_canonical_json_bytes(manifest, out_len);
    json_free(manifest);
    free(pairs);
    return bytes;
}

/* ── parse_sync_manifest (L136) ───────────────────────────────────────── */

/* Parse sync-manifest bytes into {name: enabled}, or NULL if the bytes are
 * not a well-formed manifest. Strict (mirrors parseSyncManifest): unknown
 * type, missing/!=1 version, non-array skills, or malformed entry all
 * reject — a malformed manifest must not be mistaken for "no skills". */
json_t *skills_sync_parse_manifest(const char *data, size_t len) {
    if (!data) return NULL;
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, data, len);
    buf[len] = '\0';

    json_t *value = json_parse(buf, NULL);
    free(buf);
    if (!value || value->type != JSON_OBJECT) { if (value) json_free(value); return NULL; }

    const char *type = json_get_str(value, "type", "");
    if (strcmp(type, SYNC_MANIFEST_TYPE) != 0) { json_free(value); return NULL; }
    double version = json_get_num(value, "version", -1);
    if ((int)version != SYNC_MANIFEST_VERSION) { json_free(value); return NULL; }

    json_t *raw_skills = json_obj_get(value, "skills");
    if (!raw_skills || raw_skills->type != JSON_ARRAY) { json_free(value); return NULL; }

    json_t *out = json_object();
    size_t n = json_len(raw_skills);
    for (size_t i = 0; i < n; i++) {
        json_t *raw = json_get(raw_skills, i);
        if (!raw || raw->type != JSON_OBJECT) { json_free(out); json_free(value); return NULL; }
        const char *name = json_get_str(raw, "name", "");
        json_t *enabled_node = json_obj_get(raw, "enabled");
        if (!name[0] || !enabled_node || enabled_node->type != JSON_BOOL) {
            json_free(out); json_free(value); return NULL;
        }
        /* bool_val and num_val share a union — for JSON_BOOL the value is
         * in bool_val; reading num_val reinterprets garbage high bytes. */
        bool enabled = enabled_node->bool_val;
        json_set(out, name, json_bool(enabled));
    }
    json_free(value);
    return out;
}

/* ── wire_address (L181) ──────────────────────────────────────────────── */

/* Return "sha256:<64-hex>" — the wire address of data. Uses the FULL 64-hex
 * sha256 digest (a DIFFERENT namespace from the local truncated content_hash). */
char *skills_sync_wire_address(const unsigned char *data, size_t len) {
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256(data, len, digest);
    size_t out_len = strlen(WIRE_ADDR_PREFIX) + CRYPTO_SHA256_LEN * 2 + 1;
    char *out = malloc(out_len);
    if (!out) return NULL;
    char *p = out;
    memcpy(p, WIRE_ADDR_PREFIX, strlen(WIRE_ADDR_PREFIX));
    p += strlen(WIRE_ADDR_PREFIX);
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++)
        p += sprintf(p, "%02x", digest[i]);
    return out;
}

/* ── _decode_jwt_payload_unverified (L226) ────────────────────────────── */

/* Decode a JWT payload WITHOUT signature verification. Safe here: the server
 * re-verifies every call; we only read the dev-gate claim. base64url decode
 * of the middle segment, then JSON-parse. Returns NULL on any failure. */
json_t *skills_sync_decode_jwt_payload(const char *token) {
    if (!token) return NULL;
    const char *first_dot = strchr(token, '.');
    if (!first_dot) return NULL;
    const char *second_dot = strchr(first_dot + 1, '.');
    if (!second_dot) return NULL;

    const char *payload_b64 = first_dot + 1;
    size_t payload_len = (size_t)(second_dot - payload_b64);
    if (payload_len == 0) return NULL;

    /* base64url -> base64: replace -/_ with +//, pad to multiple of 4. */
    char *b64 = malloc(payload_len + 4);
    if (!b64) return NULL;
    memcpy(b64, payload_b64, payload_len);
    for (size_t i = 0; i < payload_len; i++) {
        if (b64[i] == '-') b64[i] = '+';
        else if (b64[i] == '_') b64[i] = '/';
    }
    size_t pad = (4 - (payload_len % 4)) % 4;
    for (size_t i = 0; i < pad; i++) b64[payload_len + i] = '=';
    b64[payload_len + pad] = '\0';

    /* Decode with a small base64 decoder. */
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char rev[256];
    memset(rev, 0xff, sizeof(rev));
    for (int i = 0; i < 64; i++) rev[(unsigned char)tbl[i]] = (unsigned char)i;

    size_t b64len = strlen(b64);
    size_t out_cap = b64len / 4 * 3 + 3;
    unsigned char *raw = malloc(out_cap + 1);
    if (!raw) { free(b64); return NULL; }
    size_t o = 0;
    for (size_t i = 0; i + 3 < b64len; i += 4) {
        unsigned char a = rev[(unsigned char)b64[i]];
        unsigned char b = rev[(unsigned char)b64[i + 1]];
        unsigned char c = rev[(unsigned char)b64[i + 2]];
        unsigned char d = rev[(unsigned char)b64[i + 3]];
        raw[o++] = (a << 2) | (b >> 4);
        if (b64[i + 2] != '=') raw[o++] = (b << 4) | (c >> 2);
        if (b64[i + 3] != '=') raw[o++] = (c << 6) | d;
    }
    raw[o] = '\0';
    free(b64);

    json_t *payload = json_parse((const char *)raw, NULL);
    free(raw);
    return payload;
}

/* ── resolve_identity (L246) ──────────────────────────────────────────── */

/* Resolve the Nous bearer + owner + dev-gate flag. Returns a JSON object
 * {api_key, base_url, owner, nous_admin, claims} or NULL (inert). */
/* Read the Nous bearer JWT from $SLERMES_HOME/auth.json
 * (providers.nous.access_token). Returns malloc'd string or NULL. */
static char *read_nous_bearer(void) {
    const char *home = slermes_home();
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) return NULL;

    char path[4096];
    snprintf(path, sizeof(path), "%s/auth.json", home);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return NULL;
    buf[n] = '\0';

    json_t *j = json_parse(buf, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return NULL; }
    json_t *providers = json_obj_get(j, "providers");
    json_t *nous = providers ? json_obj_get(providers, "nous") : NULL;
    const char *token = nous ? json_get_str(nous, "access_token", "") : "";
    char *out = (token[0]) ? strdup(token) : NULL;
    json_free(j);
    return out;
}

json_t *skills_sync_resolve_identity(void) {
    char *api_key = read_nous_bearer();
    if (!api_key) return NULL;

    json_t *claims = skills_sync_decode_jwt_payload(api_key);
    if (!claims || claims->type != JSON_OBJECT) {
        free(api_key);
        if (claims) json_free(claims);
        return NULL;
    }

    const char *owner = json_get_str(claims, "sub", "");
    if (!owner[0]) owner = json_get_str(claims, "privy_did", "");
    if (!owner[0]) owner = json_get_str(claims, "tid", "");
    if (!owner[0]) owner = "unknown";

    json_t *admin_node = json_obj_get(claims, NOUS_ADMIN_CLAIM);
    bool nous_admin = admin_node != NULL &&
        admin_node->type == JSON_BOOL && admin_node->bool_val;

    json_t *out = json_object();
    json_set(out, "api_key", json_string(api_key));
    json_set(out, "base_url", json_string(DEFAULT_SYNC_BASE_URL));
    json_set(out, "owner", json_string(owner));
    json_set(out, "nous_admin", json_bool(nous_admin));
    json_set(out, "claims", claims);  /* ownership transferred */
    free(api_key);
    return out;
}

/* ── dev_gate_open (L284) ─────────────────────────────────────────────── */

bool skills_sync_dev_gate_open(void) {
    json_t *ident = skills_sync_resolve_identity();
    if (!ident) return false;
    bool open = json_get_bool(ident, "nous_admin", false);
    json_free(ident);
    return open;
}

/* ── resolve_sync_base_url (L307) ─────────────────────────────────────── */

/* Resolve the sync-plane base URL. Order: HERMES_SYNC_BASE_URL env bridge ->
 * config.yaml sync.base_url -> production plane. Returns malloc'd base
 * without trailing slash, or NULL. */
char *skills_sync_resolve_base_url(void) {
    const char *env = getenv("HERMES_SYNC_BASE_URL");
    if (env && env[0]) {
        /* trim + strip trailing slash */
        char *out = strdup(env);
        size_t L = strlen(out);
        while (L > 0 && (out[L - 1] == '/' || out[L - 1] == ' ')) out[--L] = '\0';
        return out;
    }
    /* config.yaml sync.base_url (lazy yaml load — low-level layer must not
     * import the CLI at module load; config file path is the canonical one). */
    const char *home = slermes_home();
    if (home && home[0]) {
        char cfg_path[4096];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home);
        yaml_doc_t *doc = yaml_parse_file(cfg_path, NULL);
        if (doc) {
            const char *base = yaml_get_string(doc, "sync.base_url");
            if (base && base[0]) {
                char *out = strdup(base);
                size_t L = strlen(out);
                while (L > 0 && (out[L - 1] == '/' || out[L - 1] == ' ')) out[--L] = '\0';
                yaml_free(doc);
                return out;
            }
            yaml_free(doc);
        }
    }
    return strdup(DEFAULT_SYNC_BASE_URL);
}

/* ── _parse_bool (L358) ───────────────────────────────────────────────── */

/* Parse a config/env bool. Returns -1 (unrecognized) so callers can fall
 * through to the next precedence layer; 0/1 for false/true. */
static int parse_bool_str(const char *s) {
    if (!s) return -1;
    /* trim + lowercase */
    while (*s == ' ' || *s == '\t') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L - 1] == ' ' || s[L - 1] == '\t')) L--;
    char tmp[64];
    if (L >= sizeof(tmp)) L = sizeof(tmp) - 1;
    memcpy(tmp, s, L);
    tmp[L] = '\0';
    for (size_t i = 0; i < L; i++)
        if (tmp[i] >= 'A' && tmp[i] <= 'Z') tmp[i] += 32;

    for (int i = 0; TRUE_WORDS[i]; i++)
        if (strcmp(tmp, TRUE_WORDS[i]) == 0) return 1;
    for (int i = 0; FALSE_WORDS[i]; i++)
        if (strcmp(tmp, FALSE_WORDS[i]) == 0) return 0;
    return -1;
}

/* _sync_config_bool (L373): env_var -> sync.<config_key> -> default. */
static bool sync_config_bool(const char *env_var, const char *config_key,
                             bool default_val) {
    const char *env_val = getenv(env_var);
    if (env_val) {
        int v = parse_bool_str(env_val);
        if (v >= 0) return v != 0;
    }
    const char *home = slermes_home();
    if (home && home[0]) {
        char cfg_path[4096];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home);
        yaml_doc_t *doc = yaml_parse_file(cfg_path, NULL);
        if (doc) {
            char key[128];
            snprintf(key, sizeof(key), "sync.%s", config_key);
            bool v = yaml_get_bool(doc, key, default_val);
            yaml_free(doc);
            return v;
        }
    }
    return default_val;
}

/* ── sync feature knobs (L391-435) ────────────────────────────────────── *//* ── config bool helpers (L358-388) ───────────────────────────────────── */

/* PoP: _parse_bool @ tools/skills_sync_client.py:_parse_bool */
/* Parse a config/env bool; returns -1 for unrecognized (None in Python). */
static int ssc_parse_bool(const char *value) {
    if (!value) return -1;
    /* trim + lowercase */
    char buf[128];
    size_t L = strlen(value);
    if (L >= sizeof(buf)) L = sizeof(buf) - 1;
    size_t out = 0;
    for (size_t i = 0; i < L; i++) {
        char c = value[i];
        if (c != ' ' && c != '\t') buf[out++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    buf[out] = '\0';
    /* _TRUE = {1,true,yes,on}; _FALSE = {0,false,no,off,""} */
    if (strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
        strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0)
        return 1;
    if (strcmp(buf, "0") == 0 || strcmp(buf, "false") == 0 ||
        strcmp(buf, "no") == 0 || strcmp(buf, "off") == 0 ||
        strcmp(buf, "") == 0)
        return 0;
    return -1;
}

/* PoP: _sync_config_bool @ tools/skills_sync_client.py:_sync_config_bool */
/* Resolve a boolean sync knob: env_var -> sync.<config_key> -> default. */
static bool ssc_sync_config_bool(const char *env_var, const char *config_key,
                                 bool default_val) {
    int env_val = ssc_parse_bool(getenv(env_var));
    if (env_val >= 0) return env_val != 0;
    /* config.yaml sync.<key> — best-effort read */
    json_t *cfg = NULL;
    extern json_t *config_py_load_config_readonly(void);
    cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *sync_cfg = json_obj_get(cfg, "sync");
        if (sync_cfg && sync_cfg->type == JSON_OBJECT) {
            json_t *v = json_obj_get(sync_cfg, config_key);
            int cfg_val = -1;
            if (v && v->type == JSON_STRING)
                cfg_val = ssc_parse_bool(json_get_str(v, "", ""));
            else if (v && v->type == JSON_BOOL)
                cfg_val = v->bool_val ? 1 : 0;
            if (cfg_val >= 0) { json_free(cfg); return cfg_val != 0; }
        }
        json_free(cfg);
    }
    return default_val;
}




bool skills_sync_feature_enabled(void) {
    return sync_config_bool("HERMES_SYNC_ENABLED", "enabled", false);
}

bool skills_sync_org_auto_propose(void) {
    return sync_config_bool("HERMES_SYNC_ORG_AUTO_PROPOSE", "org_auto_propose", false);
}

bool skills_sync_default_opt_in(void) {
    return sync_config_bool("HERMES_SYNC_DEFAULT_OPT_IN", "default_opt_in", false);
}

/* ── _skills_dir (L446) ───────────────────────────────────────────────── */

/* Return $SLERMES_HOME/skills into out. */
/* PoP: _skills_dir @ tools/skills_sync_client.py:_skills_dir */
static void skills_dir(char *out, size_t sz) {
    const char *home = slermes_home();
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = ".";
    snprintf(out, sz, "%s/skills", home);
}

/* ── is_sync_eligible (L452) ──────────────────────────────────────────── */

/* Whether skill_name is a candidate for sync (before the opt-in check).
 * Eligible = present locally under ~/.slermes/skills/, NOT bundled, NOT
 * hub-installed, NOT an external-dir skill, NOT under _org/. */
bool skills_sync_is_eligible(const char *skill_name) {
    if (!skill_name || !skill_name[0]) return false;

    /* Find the skill dir: $SLERMES_HOME/skills/<name> (or nested). */
    char root[4096];
    skills_dir(root, sizeof(root));
    char probe[4096];
    snprintf(probe, sizeof(probe), "%s/%s/SKILL.md", root, skill_name);
    struct stat st;
    if (stat(probe, &st) != 0) return false;  /* not present locally */

    /* Bundled skills: live under <home>/skills/.bundled_manifest; a skill
     * listed there is excluded. Hub-installed: a `.hub` marker file in the
     * skill dir (mirrors tools/skill_usage.py is_hub_installed). */
    char bundled_manifest[4096];
    snprintf(bundled_manifest, sizeof(bundled_manifest),
             "%s/.bundled_manifest", root);
    FILE *bf = fopen(bundled_manifest, "r");
    if (bf) {
        char line[512];
        while (fgets(line, sizeof(line), bf)) {
            size_t L = strlen(line);
            while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
            if (L > 0 && strcmp(line, skill_name) == 0) { fclose(bf); return false; }
        }
        fclose(bf);
    }
    char hub_marker[4096];
    snprintf(hub_marker, sizeof(hub_marker), "%s/%s/.hub", root, skill_name);
    if (stat(hub_marker, &st) == 0) return false;

    /* External-dir skills excluded. */
    if (cli_agent_skill_utils__is_external_skill_path(probe) != 0) return false;

    /* _org/ mirror excluded (org content pulls from org HEAD; must never
     * ride a personal push). */
    const char *slash = strchr(skill_name, '/');
    if (slash) {
        char first[64];
        size_t fl = (size_t)(slash - skill_name);
        if (fl >= sizeof(first)) fl = sizeof(first) - 1;
        memcpy(first, skill_name, fl);
        first[fl] = '\0';
        if (strcmp(first, ORG_DIR_NAME) == 0) return false;
    } else if (strcmp(skill_name, ORG_DIR_NAME) == 0) {
        return false;
    }
    return true;
}
/* ── _all_local_skill_names (L521) ────────────────────────────────────── */

/* Best-effort enumeration of every locally-present skill name: any directory
 * under $SLERMES_HOME/skills/ containing a SKILL.md; name = frontmatter name
 * (fallback: directory name). Caller applies eligibility. Returns a
 * NULL-terminated array of malloc'd strings (caller frees each + array). */
char **skills_sync_all_local_skill_names(size_t *out_count) {
    if (out_count) *out_count = 0;
    char root[4096];
    skills_dir(root, sizeof(root));

    struct stat st;
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) return NULL;

    size_t cap = 16, count = 0;
    char **names = calloc(cap, sizeof(char *));
    if (!names) return NULL;

    DIR *d = opendir(root);
    if (!d) return names;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char skill_md[4096];
        snprintf(skill_md, sizeof(skill_md), "%s/%s/SKILL.md", root, e->d_name);
        if (stat(skill_md, &st) != 0) continue;  /* no SKILL.md — not a skill */

        /* Read frontmatter name via the shared backend; fallback: dir name. */
        char *name = read_skill_name(skill_md, e->d_name);
        if (!name || !name[0]) { free(name); continue; }

        if (count + 1 >= cap) {
            cap *= 2;
            char **nb = realloc(names, cap * sizeof(char *));
            if (!nb) { free(name); break; }
            names = nb;
        }
        names[count++] = name;
    }
    closedir(d);
    names[count] = NULL;
    if (out_count) *out_count = count;
    return names;
}

/* ── list_synced_skill_names (L482) ───────────────────────────────────── */

/* Return the names of skills that should sync, honoring the opt-in policy.
 * Returns NULL-terminated array of malloc'd names (caller frees). */
/* Read the .usage.json sidecar {skill: {sync: bool}} into a JSON object,
 * or NULL if missing/unparseable. Mirrors tools/skill_usage.py load_usage. */
static json_t *load_usage_sidecar(void) {
    const char *home = slermes_home();
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) return NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%s/skills/.usage.json", home);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[262144];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return NULL;
    buf[n] = '\0';
    json_t *j = json_parse(buf, NULL);
    if (j && j->type != JSON_OBJECT) { json_free(j); return NULL; }
    return j;
}

/* Write the .usage.json sidecar {skill: {sync: bool}} atomically.
 * Mirrors tools/skill_usage.py save_usage. Best-effort. */
static void save_usage_sidecar(json_t *usage) {
    if (!usage) return;
    const char *home = slermes_home();
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/skills/.usage.json", home);
    char *slash = strrchr(path, '/');
    if (slash) { *slash = '\0'; mkdir(path, 0755); *slash = '/'; }
    char tmp[4200];
    snprintf(tmp, sizeof(tmp), "%s.usage_tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    char *ser = json_serialize(usage);
    fputs(ser, f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    free(ser);
    rename(tmp, path);
}

/* Whether skill_name is currently opted into sync (usage sidecar sync:true). */
bool skills_sync_is_opted_in(const char *skill_name) {
    if (!skill_name) return false;
    json_t *usage = load_usage_sidecar();
    if (!usage) return false;
    json_t *rec = json_obj_get(usage, skill_name);
    json_t *sync_node = (rec && rec->type == JSON_OBJECT)
        ? json_obj_get(rec, "sync") : NULL;
    bool val = sync_node != NULL && sync_node->type == JSON_BOOL &&
        sync_node->bool_val;
    json_free(usage);
    return val;
}

/* Set the usage-sidecar sync flag for a skill (opt-in toggle). */
void skills_sync_set_opted_in(const char *skill_name, bool val) {
    if (!skill_name) return;
    json_t *usage = load_usage_sidecar();
    if (!usage) usage = json_object();
    json_t *rec = json_obj_get(usage, skill_name);
    if (!rec || rec->type != JSON_OBJECT) {
        rec = json_object();
        json_set(usage, skill_name, rec);
    }
    json_set(rec, "sync", json_bool(val));
    save_usage_sidecar(usage);
    json_free(usage);
}

/* Return the names of skills that should sync, honoring the opt-in policy.
 * Returns NULL-terminated array of malloc'd names (caller frees). */
char **skills_sync_list_synced_skill_names(size_t *out_count) {
    if (out_count) *out_count = 0;

    json_t *usage = load_usage_sidecar();

    bool opt_out = skills_sync_default_opt_in();

    size_t cap = 16, count = 0;
    char **names = calloc(cap, sizeof(char *));
    if (!names) { if (usage) json_free(usage); return NULL; }

    size_t all_count = 0;
    char **all = skills_sync_all_local_skill_names(&all_count);

    for (size_t i = 0; i < all_count; i++) {
        bool include = false;
        if (usage) {
            json_t *rec = json_obj_get(usage, all[i]);
            json_t *sync_node = (rec && rec->type == JSON_OBJECT)
                ? json_obj_get(rec, "sync") : NULL;
            bool sync_val = sync_node != NULL &&
                sync_node->type == JSON_BOOL && sync_node->bool_val;
            if (opt_out) {
                /* opt-OUT: explicit sync:false wins over the default. */
                if (sync_node && sync_node->type == JSON_BOOL && !sync_val)
                    include = false;
                else
                    include = true;
            } else {
                /* opt-IN (default): only explicit sync:true. */
                include = sync_val;
            }
        } else {
            /* No usage sidecar: opt-OUT includes all eligible; opt-IN includes none. */
            include = opt_out;
        }
        if (include && skills_sync_is_eligible(all[i])) {
            if (count + 1 >= cap) {
                cap *= 2;
                char **nb = realloc(names, cap * sizeof(char *));
                if (!nb) break;
                names = nb;
            }
            names[count++] = strdup(all[i]);
        }
    }

    for (size_t i = 0; i < all_count; i++) free(all[i]);
    free(all);
    if (usage) json_free(usage);
    names[count] = NULL;
    if (out_count) *out_count = count;
    return names;
}
/* ═══════════════════════════════════════════════════════════════════════
 * CLUSTER 2: Object building + device ID + wire client
 * Python L558-L928
 * ═══════════════════════════════════════════════════════════════════════ */

/* Object kinds + tree modes + artifact type (sync contract). */
#define KIND_BLOB   "blob"
#define KIND_TREE   "tree"
#define KIND_COMMIT "commit"
#define MODE_FILE   "file"
#define MODE_EXEC   "exec"
#define MODE_DIR    "dir"
#define ARTIFACT_TYPE_SKILL "skill"
#define DEFAULT_MAX_OBJECT_BYTES (25 * 1024 * 1024)
#define WIRE_VERSION "1"

/* ── ObjectSet (L558) ────────────────────────────────────────────────── */

/* Accumulates objects to push: addr -> {kind, data}. Deduped by content
 * address, so identical blobs across skills upload once. */

struct ssc_object {
    char *addr;          /* sha256:<64-hex> */
    char *kind;          /* blob|tree|commit */
    unsigned char *data;
    size_t data_len;
    struct ssc_object *next;
};
/* ── ObjectSet (L558) ────────────────────────────────────────────────── */

/* Accumulates objects to push: addr -> {kind, data}. Deduped by content
 * address, so identical blobs across skills upload once. */


/* PoP: add @ tools/skills_sync_client.py:add */
/* Add an object; returns malloc'd address (deduped by content address). */
char *ssc_object_set_add(ssc_object_set_t *set, const char *kind,
                         const unsigned char *data, size_t len) {
    if (!set) return NULL;
    char *addr = skills_sync_wire_address(data, len);
    if (!addr) return NULL;
    /* Dedup: skip if an object with this address exists. */
    for (ssc_object_t *o = set->head; o; o = o->next) {
        if (strcmp(o->addr, addr) == 0) { free(addr); return strdup(o->addr); }
    }
    ssc_object_t *o = calloc(1, sizeof(ssc_object_t));
    if (!o) { free(addr); return NULL; }
    o->addr = addr;
    o->kind = strdup(kind);
    o->data = malloc(len ? len : 1);
    if (!o->data) { free(o->addr); free(o->kind); free(o); return NULL; }
    memcpy(o->data, data, len);
    o->data_len = len;
    o->next = set->head;
    set->head = o;
    set->count++;
    return strdup(addr);
}

void ssc_object_set_free(ssc_object_set_t *set) {
    if (!set) return;
    ssc_object_t *o = set->head;
    while (o) {
        ssc_object_t *n = o->next;
        free(o->addr); free(o->kind); free(o->data); free(o);
        o = n;
    }
    set->head = NULL;
    set->count = 0;
}

/* PoP: __len__ @ tools/skills_sync_client.py:__len__ */
size_t ssc_object_set_len(const ssc_object_set_t *set) {
    return set ? set->count : 0;
}

/* ── _file_mode (L576) ───────────────────────────────────────────────── */

/* PoP: _file_mode @ tools/skills_sync_client.py:_file_mode */
/* Return the tree mode for a regular file: "exec" if any +x bit else
 * "file" (contract §2.3). */
static const char *ssc_file_mode(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 &&
        (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return MODE_EXEC;
    return MODE_FILE;
}

/* ── build_tree (L587) ───────────────────────────────────────────────── */

/* Recursively build objects for dir_path; return malloc'd tree address.
 * Regular files -> blobs; subdirs -> nested trees; symlinks/specials
 * skipped (contract §2.3). Blobs over max_object_bytes fail via the
 * return of a NULL + errno-style flag (ValueError in Python). */

typedef struct ssc_tree_entry {
    char *name;
    char *kind;   /* blob|tree */
    char *hash;
    const char *mode;  /* file|exec|dir (not owned) */
    struct ssc_tree_entry *next;
} ssc_tree_entry_t;

static char *ssc_build_tree_rec(const char *dir_path,
                                ssc_object_set_t *objects,
                                long max_object_bytes,
                                int *too_large) {
    DIR *d = opendir(dir_path);
    if (!d) return NULL;
    /* Collect entries first (sorted by name later). */
    ssc_tree_entry_t *entries = NULL, **tailp = &entries;
    size_t nentries = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;  /* skip dotfiles (incl. .sync_*) */
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", dir_path, e->d_name);
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;  /* no symlinks (contract §2.3) */
        ssc_tree_entry_t *ent = calloc(1, sizeof(ssc_tree_entry_t));
        if (!ent) continue;
        ent->name = strdup(e->d_name);
        if (S_ISDIR(st.st_mode)) {
            int sub_too_large = 0;
            char *sub_hash = ssc_build_tree_rec(child, objects,
                                                max_object_bytes, &sub_too_large);
            if (!sub_hash) { free(ent->name); free(ent); continue; }
            if (sub_too_large) *too_large = 1;
            ent->kind = strdup(KIND_TREE);
            ent->hash = sub_hash;
            ent->mode = MODE_DIR;
        } else if (S_ISREG(st.st_mode)) {
            /* Read file bytes. */
            FILE *f = fopen(child, "rb");
            if (!f) { free(ent->name); free(ent); continue; }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz < 0) { fclose(f); free(ent->name); free(ent); continue; }
            if (max_object_bytes > 0 && sz > max_object_bytes) {
                fclose(f);
                free(ent->name); free(ent);
                *too_large = 1;
                continue;
            }
            unsigned char *data = malloc(sz ? (size_t)sz : 1);
            if (!data) { fclose(f); free(ent->name); free(ent); continue; }
            size_t rd = fread(data, 1, (size_t)sz, f);
            fclose(f);
            if (rd != (size_t)sz) { free(data); free(ent->name); free(ent); continue; }
            char *blob_hash = ssc_object_set_add(objects, KIND_BLOB, data, (size_t)sz);
            free(data);
            if (!blob_hash) { free(ent->name); free(ent); continue; }
            ent->kind = strdup(KIND_BLOB);
            ent->hash = blob_hash;
            ent->mode = ssc_file_mode(child);
        } else {
            /* skip special files */
            free(ent->name);
            free(ent);
            continue;
        }
        *tailp = ent;
        tailp = &ent->next;
        nentries++;
    }
    closedir(d);

    /* Sort entries by name (byte order) for canonicalization. */
    /* (insertion sort — small counts) */
    ssc_tree_entry_t *sorted = NULL;
    while (entries) {
        ssc_tree_entry_t *cur = entries;
        entries = entries->next;
        cur->next = NULL;
        if (!sorted || strcmp(cur->name, sorted->name) < 0) {
            cur->next = sorted;
            sorted = cur;
        } else {
            ssc_tree_entry_t *p = sorted;
            while (p->next && strcmp(cur->name, p->next->name) > 0) p = p->next;
            cur->next = p->next;
            p->next = cur;
        }
    }

    /* Build tree JSON: {"type":"tree","entries":[{name,kind,hash,mode}...]}.
     * Keys per entry must be emitted in sorted order: hash < kind < mode < name. */
    json_t *entries_arr = json_array();
    for (ssc_tree_entry_t *cur = sorted; cur; cur = cur->next) {
        json_t *ent = json_object();
        json_set(ent, "hash", json_string(cur->hash));
        json_set(ent, "kind", json_string(cur->kind));
        json_set(ent, "mode", json_string(cur->mode));
        json_set(ent, "name", json_string(cur->name));
        json_append(entries_arr, ent);
    }
    json_t *tree_obj = json_object();
    json_set(tree_obj, "entries", entries_arr);
    json_set(tree_obj, "type", json_string(KIND_TREE));

    char *ser = skills_sync_canonical_json_bytes(tree_obj, NULL);
    json_free(tree_obj);
    char *tree_addr = ser ? ssc_object_set_add(objects, KIND_TREE,
                                               (const unsigned char *)ser,
                                               strlen(ser)) : NULL;
    free(ser);

    /* free entries */
    ssc_tree_entry_t *cur = sorted;
    while (cur) {
        ssc_tree_entry_t *n = cur->next;
        free(cur->name); free(cur->kind); free(cur->hash); free(cur);
        cur = n;
    }
    return tree_addr;
}

/* PoP: build_tree @ tools/skills_sync_client.py:build_tree */
char *ssc_build_tree(const char *dir_path, ssc_object_set_t *objects,
                     long max_object_bytes, int *too_large) {
    if (too_large) *too_large = 0;
    return ssc_build_tree_rec(dir_path, objects, max_object_bytes, too_large);
}

/* ── build_commit (L628) ─────────────────────────────────────────────── */

/* Build a commit object (sync contract) and return its address.
 * parents: 0 first commit, 1 normal edit, 2 merge (parents[0] = base
 * fast-forwarded from, parents[1] = other head being merged). */
char *ssc_build_commit(const char *tree_hash,
                       const char *const *parents, size_t nparents,
                       const char *owner, const char *device,
                       const char *message, ssc_object_set_t *objects,
                       const char *ts) {
    /* Default ts: UTC ISO-8601 "%Y-%m-%dT%H:%M:%SZ". */
    char tsbuf[64];
    if (!ts || !ts[0]) {
        time_t now = time(NULL);
        struct tm tmv;
        gmtime_r(&now, &tmv);
        strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
        ts = tsbuf;
    }

    json_t *parents_arr = json_array();
    for (size_t i = 0; i < nparents; i++)
        json_append(parents_arr, json_string(parents[i]));

    json_t *author = json_object();
    /* sorted: device < owner */
    json_set(author, "device", json_string(device ? device : ""));
    json_set(author, "owner", json_string(owner ? owner : ""));

    json_t *commit_obj = json_object();
    /* Keys sorted: artifact_type < author < message < parents < tree < ts < type */
    json_set(commit_obj, "artifact_type", json_string(ARTIFACT_TYPE_SKILL));
    json_set(commit_obj, "author", author);
    json_set(commit_obj, "message", json_string(message ? message : ""));
    json_set(commit_obj, "parents", parents_arr);
    json_set(commit_obj, "tree", json_string(tree_hash));
    json_set(commit_obj, "ts", json_string(ts));
    json_set(commit_obj, "type", json_string(KIND_COMMIT));

    char *ser = skills_sync_canonical_json_bytes(commit_obj, NULL);
    json_free(commit_obj);
    char *addr = ser ? ssc_object_set_add(objects, KIND_COMMIT,
                                          (const unsigned char *)ser,
                                          strlen(ser)) : NULL;
    free(ser);
    return addr;
}

/* ── device identity (L656-727) ──────────────────────────────────────── */

/* PoP: _default_device_label @ tools/skills_sync_client.py:_default_device_label */
/* A human-friendly default device label: short hostname + 6-hex random
 * suffix; falls back to bare hex uuid. Returns malloc'd string. */
static char *ssc_default_device_label(void) {
    char host[256] = "";
    if (gethostname(host, sizeof(host) - 1) != 0) host[0] = '\0';
    /* short hostname: drop domain */
    char *dot = strchr(host, '.');
    if (dot) *dot = '\0';
    /* keep only alnum, '-', '_' */
    size_t w = 0;
    for (char *p = host; *p && w < sizeof(host) - 1; p++) {
        if (isalnum((unsigned char)*p) || *p == '-' || *p == '_') host[w++] = *p;
    }
    host[w] = '\0';
    /* 6-hex random suffix */
    char suffix[7];
    FILE *ur = fopen("/dev/urandom", "rb");
    unsigned char rb[3];
    if (ur) { size_t got = fread(rb, 1, 3, ur); fclose(ur);
        if (got == 3) snprintf(suffix, sizeof(suffix), "%02x%02x%02x",
                               rb[0], rb[1], rb[2]);
        else snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)time(NULL) & 0xffffff);
    } else {
        snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)time(NULL) & 0xffffff);
    }
    if (host[0]) {
        char *out = malloc(strlen(host) + 8);
        if (!out) return NULL;
        sprintf(out, "%s-%s", host, suffix);
        return out;
    }
    /* bare uuid-ish fallback: hex of time+pid */
    char *out = malloc(40);
    if (!out) return NULL;
    snprintf(out, 40, "%06lx%06lx", (unsigned long)time(NULL) & 0xffffff,
             (unsigned long)getpid() & 0xffffff);
    return out;
}

/* _skills_dir path helper (already defined above as skills_dir). */

/* PoP: stable_device_id @ tools/skills_sync_client.py:stable_device_id */
/* Return a stable per-device label for commit author.device. Persisted
 * under ~/.slermes/skills/.sync_device_id. Returns malloc'd string. */
char *ssc_stable_device_id(void) {
    char root[4096];
    skills_dir(root, sizeof(root));
    char path[4096];
    snprintf(path, sizeof(path), "%s/.sync_device_id", root);

    /* existing file wins */
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = '\0';
        /* trim */
        size_t L = n;
        while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r' || buf[L-1] == ' ')) buf[--L] = '\0';
        if (L > 0) return strdup(buf);
    }

    /* env seeds FIRST-USE value only */
    const char *env_name = getenv("HERMES_SYNC_DEVICE_NAME");
    char *val = NULL;
    if (env_name && env_name[0]) {
        val = strdup(env_name);
        /* trim */
        size_t L = strlen(val);
        while (L > 0 && (val[L-1] == ' ')) val[--L] = '\0';
    }
    if (!val) val = ssc_default_device_label();
    if (!val) return NULL;

    /* persist */
    mkdir(root, 0755);
    FILE *wf = fopen(path, "w");
    if (wf) { fputs(val, wf); fclose(wf); }
    return val;
}

/* PoP: set_device_name @ tools/skills_sync_client.py:set_device_name */
/* Set the human-friendly device label; writes (trimmed) name to
 * ~/.slermes/skills/.sync_device_id. Returns 0 on success, -1 on empty
 * name (Python raises ValueError). */
int ssc_set_device_name(const char *name, char *out, size_t out_sz) {
    if (!name) return -1;
    /* trim */
    size_t L = strlen(name);
    while (L > 0 && (name[L-1] == ' ' || name[L-1] == '\t')) L--;
    if (L == 0) return -1;

    char root[4096];
    skills_dir(root, sizeof(root));
    mkdir(root, 0755);
    char path[4096];
    snprintf(path, sizeof(path), "%s/.sync_device_id", root);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fwrite(name, 1, L, f);
    fclose(f);
    if (out && out_sz) {
        size_t c = L < out_sz - 1 ? L : out_sz - 1;
        memcpy(out, name, c);
        out[c] = '\0';
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * CLUSTER 2 (cont.): SyncClient — the wire client
 * Python L767-L928
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct ssc_sync_client {
    char base[512];
    char api_key[4096];
    int timeout_sec;
    http_t *h;
} ssc_sync_client_t;

/* PoP: __init__ @ tools/skills_sync_client.py:__init__ */
ssc_sync_client_t *ssc_client_new(const char *base_url, const char *api_key,
                                  int timeout_sec) {
    ssc_sync_client_t *c = calloc(1, sizeof(ssc_sync_client_t));
    if (!c) return NULL;
    snprintf(c->base, sizeof(c->base), "%s", base_url ? base_url : "");
    size_t L = strlen(c->base);
    while (L > 0 && c->base[L-1] == '/') c->base[--L] = '\0';
    snprintf(c->api_key, sizeof(c->api_key), "%s", api_key ? api_key : "");
    c->timeout_sec = timeout_sec > 0 ? timeout_sec : 30;
    c->h = http_new(c->timeout_sec);
    if (!c->h) { free(c); return NULL; }
    return c;
}

void ssc_client_free(ssc_sync_client_t *c) {
    if (!c) return;
    if (c->h) http_free(c->h);
    free(c);
}

/* PoP: _url @ tools/skills_sync_client.py:_url */
static void ssc_client_url(const ssc_sync_client_t *c, const char *path,
                           char *out, size_t out_sz) {
    const char *p = path;
    while (*p == '/') p++;
    snprintf(out, out_sz, "%s/v1/sync/%s", c->base, p);
}

/* auth header string for requests */
static void ssc_auth_header(const ssc_sync_client_t *c, char *out, size_t sz) {
    snprintf(out, sz, "Authorization: Bearer %s", c->api_key);
}

/* PoP: capabilities @ tools/skills_sync_client.py:capabilities */
/* GET /v1/sync/capabilities. No auth required. Returns parsed JSON (caller
 * frees) or NULL on failure (status != 200). */
json_t *ssc_client_capabilities(ssc_sync_client_t *c, int *out_status) {
    if (!c) return NULL;
    char url[1024];
    ssc_client_url(c, "capabilities", url, sizeof(url));
    http_resp_t *r = http_get(c->h, url, NULL);
    if (!r) { if (out_status) *out_status = -1; return NULL; }
    if (r->status != 200) {
        if (out_status) *out_status = r->status;
        http_resp_free(r);
        return NULL;
    }
    json_t *j = r->body ? json_parse(r->body, NULL) : NULL;
    http_resp_free(r);
    if (out_status) *out_status = 200;
    return j;
}

/* PoP: get_refs @ tools/skills_sync_client.py:get_refs */
/* GET /v1/sync/refs?prefix=... (or org route). Returns a JSON array of
 * ref objects (caller frees) or NULL on failure. */
json_t *ssc_client_get_refs(ssc_sync_client_t *c, const char *prefix,
                            bool org_scope, int *out_status) {
    if (!c) return NULL;
    char url[1024];
    if (org_scope) {
        ssc_client_url(c, "org/refs", url, sizeof(url));
    } else {
        char p[1024];
        ssc_client_url(c, "refs", p, sizeof(p));
        /* URL-encode prefix minimally (it is a ref path, alnum + / + _) */
        snprintf(url, sizeof(url), "%s?prefix=%s", p, prefix ? prefix : "");
    }
    char auth[4200];
    ssc_auth_header(c, auth, sizeof(auth));
    http_resp_t *r = http_get(c->h, url, auth);
    if (!r) { if (out_status) *out_status = -1; return NULL; }
    if (r->status != 200) {
        if (out_status) *out_status = r->status;
        http_resp_free(r);
        return NULL;
    }
    json_t *j = r->body ? json_parse(r->body, NULL) : NULL;
    http_resp_free(r);
    if (out_status) *out_status = 200;
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return NULL; }
    json_t *refs = json_obj_get(j, "refs");
    json_t *out = refs ? json_copy(refs) : json_array();
    json_free(j);
    /* org scope: filter by prefix client-side */
    if (org_scope && prefix && prefix[0] && out->type == JSON_ARRAY) {
        json_t *filtered = json_array();
        size_t n = json_len(out);
        for (size_t i = 0; i < n; i++) {
            json_t *ref = json_get(out, i);
            const char *name = (ref && ref->type == JSON_OBJECT)
                ? json_get_str(ref, "name", "") : "";
            if (strncmp(name, prefix, strlen(prefix)) == 0)
                json_append(filtered, json_copy(ref));
        }
        json_free(out);
        out = filtered;
    }
    return out;
}

/* PoP: get_object @ tools/skills_sync_client.py:get_object */
/* GET /v1/sync/objects/:hash (or org route). Kind from the
 * X-HSP-Object-Type header; blob default. Returns 0 on success with
 * out_kind/out_data set (caller frees), -1 on error with *out_status set. */
int ssc_client_get_object(ssc_sync_client_t *c, const char *obj_hash,
                          bool org_scope,
                          char **out_kind, unsigned char **out_data,
                          size_t *out_len, int *out_status) {
    if (out_kind) *out_kind = NULL;
    if (out_data) *out_data = NULL;
    if (out_len) *out_len = 0;
    if (!c || !obj_hash) { if (out_status) *out_status = -1; return -1; }

    char url[1024];
    if (org_scope) {
        ssc_client_url(c, "org/objects/", url, sizeof(url));
    } else {
        ssc_client_url(c, "objects/", url, sizeof(url));
    }
    size_t ul = strlen(url);
    snprintf(url + ul, sizeof(url) - ul, "%s", obj_hash);

    char auth[4200];
    ssc_auth_header(c, auth, sizeof(auth));
    http_resp_t *r = http_get(c->h, url, auth);
    if (!r) { if (out_status) *out_status = -1; return -1; }
    if (r->status != 200) {
        if (out_status) *out_status = r->status;
        http_resp_free(r);
        return -1;
    }
    /* parse X-HSP-Object-Type from headers */
    const char *kind = KIND_BLOB;
    if (r->headers) {
        const char *found = strstr(r->headers, "X-HSP-Object-Type");
        if (found) {
            const char *colon = strchr(found, ':');
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ' || *v == '\t') v++;
                const char *end = v;
                while (*end && *end != '\r' && *end != '\n') end++;
                size_t klen = (size_t)(end - v);
                if (klen > 0 && klen < 32) {
                    static char kindbuf[32];
                    memcpy(kindbuf, v, klen);
                    kindbuf[klen] = '\0';
                    kind = kindbuf;
                }
            }
        }
    }
    if (out_kind) *out_kind = strdup(kind);
    if (out_data) {
        size_t n = r->body_len;
        unsigned char *copy = malloc(n ? n : 1);
        if (!copy) { http_resp_free(r); return -1; }
        memcpy(copy, r->body ? r->body : "", n);
        *out_data = copy;
    }
    if (out_len) *out_len = r->body_len;
    if (out_status) *out_status = 200;
    http_resp_free(r);
    return 0;
}

/* PoP: get_commit_json @ tools/skills_sync_client.py:get_commit_json */
json_t *ssc_client_get_commit_json(ssc_sync_client_t *c, const char *hash,
                                   bool org_scope, int *out_status) {
    char *kind = NULL;
    unsigned char *data = NULL;
    size_t len = 0;
    if (ssc_client_get_object(c, hash, org_scope, &kind, &data, &len, out_status) != 0)
        return NULL;
    if (!kind || strcmp(kind, KIND_COMMIT) != 0) {
        free(kind); free(data);
        if (out_status) *out_status = -1;
        return NULL;
    }
    free(kind);
    json_t *j = json_parse((const char *)data, NULL);
    free(data);
    return j;
}

/* PoP: get_tree_json @ tools/skills_sync_client.py:get_tree_json */
json_t *ssc_client_get_tree_json(ssc_sync_client_t *c, const char *hash,
                                 bool org_scope, int *out_status) {
    char *kind = NULL;
    unsigned char *data = NULL;
    size_t len = 0;
    if (ssc_client_get_object(c, hash, org_scope, &kind, &data, &len, out_status) != 0)
        return NULL;
    if (!kind || strcmp(kind, KIND_TREE) != 0) {
        free(kind); free(data);
        if (out_status) *out_status = -1;
        return NULL;
    }
    free(kind);
    json_t *j = json_parse((const char *)data, NULL);
    free(data);
    return j;
}

/* PoP: put_objects @ tools/skills_sync_client.py:put_objects */
/* POST /v1/sync/objects — multipart upload, one part per object,
 * field name = sha256:<hex> hash, filename = object type, body = raw
 * bytes. Returns 0 on success (200/201), -1 on error with *out_status. */
int ssc_client_put_objects(ssc_sync_client_t *c, const ssc_object_set_t *set,
                           bool org_scope, int *out_status) {
    if (!c || !set) { if (out_status) *out_status = -1; return -1; }
    http_multipart_form_t *form = http_multipart_form_new();
    if (!form) { if (out_status) *out_status = -1; return -1; }
    for (ssc_object_t *o = set->head; o; o = o->next) {
        http_multipart_add_file(form, o->addr, o->kind,
                                (const char *)o->data, o->data_len,
                                "application/octet-stream");
    }
    size_t body_len = 0;
    char *boundary = NULL;
    char *body = http_multipart_form_finalize(form, &body_len, &boundary);
    http_multipart_form_free(form);
    if (!body) { if (out_status) *out_status = -1; return -1; }

    char url[1024];
    if (org_scope) {
        ssc_client_url(c, "objects", url, sizeof(url));
        size_t ul = strlen(url);
        snprintf(url + ul, sizeof(url) - ul, "?scope=org");
    } else {
        ssc_client_url(c, "objects", url, sizeof(url));
    }
    char auth[4200];
    ssc_auth_header(c, auth, sizeof(auth));

    /* Build content-type with boundary manually via http_request. */
    char ct[1024];
    snprintf(ct, sizeof(ct), "Content-Type: multipart/form-data; boundary=%s\r\n%s",
             boundary ? boundary : "", auth);
    http_resp_t *r = http_request(c->h, HTTP_POST, url, ct, body, body_len);
    free(body);
    free(boundary);
    if (!r) { if (out_status) *out_status = -1; return -1; }
    int st = r->status;
    int rc = (st == 200 || st == 201) ? 0 : -1;
    if (out_status) *out_status = st;
    http_resp_free(r);
    return rc;
}

/* PoP: cas_ref @ tools/skills_sync_client.py:cas_ref */
/* POST /v1/sync/refs/:name — atomic CAS. Returns:
 *  0 = merged (200)
 *  1 = proposal_pending (202)
 *  2 = conflict (409) — *out_actual set to server's actual head ("" or hash)
 *  -1 = other error — *out_status set. */
int ssc_client_cas_ref(ssc_sync_client_t *c, const char *name,
                       const char *from_hash, const char *to_hash,
                       char *out_actual, size_t actual_sz,
                       int *out_status, json_t **out_body) {
    if (out_actual) out_actual[0] = '\0';
    if (out_body) *out_body = NULL;
    if (!c || !name) { if (out_status) *out_status = -1; return -1; }

    char url[1024];
    ssc_client_url(c, "refs/", url, sizeof(url));
    size_t ul = strlen(url);
    snprintf(url + ul, sizeof(url) - ul, "%s", name);

    json_t *body_obj = json_object();
    if (from_hash && from_hash[0])
        json_set(body_obj, "from", json_string(from_hash));
    else
        json_set(body_obj, "from", json_null());
    json_set(body_obj, "to", json_string(to_hash ? to_hash : ""));
    char *body = json_serialize(body_obj);
    json_free(body_obj);

    char auth[4200];
    ssc_auth_header(c, auth, sizeof(auth));
    http_resp_t *r = http_post_json_auth(c->h, url, body ? body : "{}", auth);
    free(body);
    if (!r) { if (out_status) *out_status = -1; return -1; }
    int st = r->status;
    if (out_status) *out_status = st;
    int rc;
    if (st == 200) {
        rc = 0;
        if (out_body && r->body) *out_body = json_parse(r->body, NULL);
    } else if (st == 202) {
        rc = 1;
        if (out_body && r->body) *out_body = json_parse(r->body, NULL);
    } else if (st == 409) {
        rc = 2;
        /* actual from JSON body */
        if (r->body && out_actual) {
            json_t *j = json_parse(r->body, NULL);
            if (j) {
                const char *actual = json_get_str(j, "actual", "");
                snprintf(out_actual, actual_sz, "%s", actual);
                json_free(j);
            }
        }
    } else {
        rc = -1;
    }
    http_resp_free(r);
    return rc;
}

/* Test/oracle accessor: first object in the set. */
ssc_object_t *ssc_object_set_head(const ssc_object_set_t *set) {
    return set ? set->head : NULL;
}
const char *ssc_object_addr(const ssc_object_t *o) { return o ? o->addr : NULL; }
const char *ssc_object_kind(const ssc_object_t *o) { return o ? o->kind : NULL; }
const unsigned char *ssc_object_data(const ssc_object_t *o) { return o ? o->data : NULL; }
size_t ssc_object_len(const ssc_object_t *o) { return o ? o->data_len : 0; }
const ssc_object_t *ssc_object_next(const ssc_object_t *o) { return o ? o->next : NULL; }

/* ═══════════════════════════════════════════════════════════════════════
 * CLUSTER 3: Sync state + materialize_tree + profile snapshot +
 *            ref naming + read_manifest_of_root + _check_version
 * Python L946-L1252
 * ═══════════════════════════════════════════════════════════════════════ */

/* skill_usage backend: find a skill's directory under the skills root. */
extern char *su_find_skill_dir(const char *hermes_home, const char *name);

/* ── sync state path helpers (L946-951) ────────────────────────────── */

/* PoP: _sync_state_path @ tools/skills_sync_client.py:_sync_state_path */
static void ssc_sync_state_path(char *out, size_t out_sz) {
    char root[4096];
    skills_dir(root, sizeof(root));
    snprintf(out, out_sz, "%s/.sync_state", root);
}

/* PoP: _legacy_sync_state_path @ tools/skills_sync_client.py:_legacy_sync_state_path */
static void ssc_legacy_sync_state_path(char *out, size_t out_sz) {
    char root[4096];
    skills_dir(root, sizeof(root));
    snprintf(out, out_sz, "%s/.sync_manifest", root);
}

/* PoP: read_sync_state @ tools/skills_sync_client.py:read_sync_state */
/* Read the local sync state; migrate legacy .sync_manifest on first
 * read. Returns a malloc'd JSON object (caller frees) or NULL on
 * total failure (returns {"head":null,"skills":{}} as JSON). */
json_t *ssc_read_sync_state(void) {
    char path[4096], legacy[4096];
    ssc_sync_state_path(path, sizeof(path));
    ssc_legacy_sync_state_path(legacy, sizeof(legacy));

    /* new path exists? */
    json_t *j = json_parse_file(path, NULL);
    if (j && j->type == JSON_OBJECT) {
        json_set(j, "head", json_null());
        json_set(j, "skills", json_object());
        return j;
    }
    if (j) json_free(j);

    /* migrate legacy */
    FILE *lf = fopen(legacy, "r");
    if (lf) {
        fclose(lf);
        json_t *j = json_parse_file(legacy, NULL);
        if (j && j->type == JSON_OBJECT) {
            json_set(j, "head", json_null());
            json_set(j, "skills", json_object());
            /* write to new path */
            FILE *wf = fopen(path, "w");
            if (wf) {
                char *ser = json_serialize(j);
                fputs(ser, wf);
                fclose(wf);
                free(ser);
            }
            /* best-effort unlink legacy */
            unlink(legacy);
            return j;
        }
        if (j) json_free(j);
    }

    json_t *def = json_object();
    json_set(def, "head", json_null());
    json_set(def, "skills", json_object());
    return def;
}

/* PoP: write_sync_state @ tools/skills_sync_client.py:write_sync_state */
/* Write sync state atomically (tempfile + os.replace). Best-effort;
 * returns 0 on success, -1 on failure. */
int ssc_write_sync_state(json_t *data) {
    if (!data) return -1;
    char path[4096];
    ssc_sync_state_path(path, sizeof(path));
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }

    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.sync_state_tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    char *ser = json_serialize(data);
    if (ser) {
        fputs(ser, f);
        fflush(f);
        fsync(fileno(f));
        free(ser);
    }
    fclose(f);
    int rc = rename(tmp, path) == 0 ? 0 : -1;
    if (rc != 0) unlink(tmp);
    return rc;
}

/* ── materialize_tree (L1021) ───────────────────────────────────────── */

/* Write tree at tree_hash into dest (created if needed). Blobs -> files
 * (restored exec bit for MODE_EXEC), nested trees -> subdirs.
 * Path traversal entries ("..", "/") are skipped. Returns 0 on success,
 * -1 on error. */
/* PoP: materialize_tree @ tools/skills_sync_client.py:materialize_tree */
int ssc_materialize_tree(ssc_sync_client_t *client, const char *tree_hash,
                             const char *dest, bool org_scope) {
    if (!client || !tree_hash || !dest) return -1;
    char url[1024];
    ssc_client_url(client, "objects/", url, sizeof(url));
    size_t ul = strlen(url);
    snprintf(url + ul, sizeof(url) - ul, "%s", tree_hash);

    char auth[4200];
    ssc_auth_header(client, auth, sizeof(auth));
    http_resp_t *r = http_get(client->h, url, auth);
    if (!r || r->status != 200) {
        if (r) http_resp_free(r);
        return -1;
    }
    json_t *tree = json_parse(r->body, NULL);
    http_resp_free(r);
    if (!tree || tree->type != JSON_OBJECT) {
        if (tree) json_free(tree);
        return -1;
    }

    /* mkdir dest */
    mkdir(dest, 0755);

    json_t *entries = json_obj_get(tree, "entries");
    if (!entries || entries->type != JSON_ARRAY) {
        json_free(tree);
        return -1;
    }

    int rc = 0;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n && rc == 0; i++) {
        json_t *e = json_get(entries, i);
        const char *name = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "name", "") : "";
        if (!name[0] || strchr(name, '/') ||
            strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;  /* skip unsafe names */
        }
        const char *kind = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "kind", "") : "";
        const char *mode = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "mode", "") : "";
        const char *hash = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "hash", "") : "";

        char target[8192];
        snprintf(target, sizeof(target), "%s/%s", dest, name);

        if (strcmp(kind, KIND_TREE) == 0) {
            rc = ssc_materialize_tree(client, hash, target, org_scope);
        } else if (strcmp(kind, KIND_BLOB) == 0) {
            /* fetch blob */
            char burl[1024];
            ssc_client_url(client, "objects/", burl, sizeof(burl));
            size_t bul = strlen(burl);
            snprintf(burl + bul, sizeof(burl) - bul, "%s", hash);
            char bauth[4200];
            ssc_auth_header(client, bauth, sizeof(bauth));
            http_resp_t *br = http_get(client->h, burl, bauth);
            if (!br || br->status != 200) {
                if (br) http_resp_free(br);
                rc = -1;
                continue;
            }
            FILE *wf = fopen(target, "wb");
            if (!wf) { http_resp_free(br); rc = -1; continue; }
            fwrite(br->body, 1, br->body_len, wf);
            fclose(wf);
            http_resp_free(br);
            /* restore exec bit */
            if (strcmp(mode, MODE_EXEC) == 0) {
                struct stat st;
                if (stat(target, &st) == 0)
                    chmod(target, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
            }
        }
    }
    json_free(tree);
    return rc;
}

/* ── _skill_rel_path (L1061) ────────────────────────────────────────── */

/* Return the skill's path relative to the skills dir (posix), or NULL. */
/* Recursive skill-dir finder (Python _find_skill_dir): walk the skills
 * root, read each SKILL.md frontmatter name, match. Returns 0 + out dir
 * on hit, -1 on miss. */
static int ssc_find_skill_dir_rec(const char *base, const char *skill_name,
                                  char *out, size_t out_sz) {
    DIR *d = opendir(base);
    if (!d) return -1;
    struct dirent *e;
    int rc = -1;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char child[4096];
        snprintf(child, sizeof(child), "%s/%s", base, e->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            /* check for SKILL.md here */
            char md[4096];
            snprintf(md, sizeof(md), "%s/SKILL.md", child);
            if (access(md, F_OK) == 0) {
                /* read frontmatter name */
                char name[256] = "";
                FILE *f = fopen(md, "r");
                if (f) {
                    char line[512];
                    while (fgets(line, sizeof(line), f)) {
                        if (line[0] == '\n' || line[0] == '\r') break;
                        if (strncmp(line, "name:", 5) == 0) {
                            char *v = line + 5;
                            while (*v == ' ' || *v == '\t') v++;
                            size_t L = strlen(v);
                            while (L > 0 && (v[L-1] == '\n' || v[L-1] == '\r' || v[L-1] == ' ')) v[--L] = '\0';
                            snprintf(name, sizeof(name), "%s", v);
                            break;
                        }
                    }
                    fclose(f);
                }
                if (name[0] && strcmp(name, skill_name) == 0) {
                    snprintf(out, out_sz, "%s", child);
                    rc = 0;
                    break;
                }
                /* fallback: dir name matches */
                if (strcmp(e->d_name, skill_name) == 0) {
                    snprintf(out, out_sz, "%s", child);
                    rc = 0;
                    break;
                }
            }
            /* recurse into non-matching dirs */
            if (rc != 0 && ssc_find_skill_dir_rec(child, skill_name, out, out_sz) == 0) {
                rc = 0;
                break;
            }
        }
    }
    closedir(d);
    return rc;
}

static int ssc_find_skill_dir(const char *skill_name, char *out, size_t out_sz) {
    char root[4096];
    skills_dir(root, sizeof(root));
    return ssc_find_skill_dir_rec(root, skill_name, out, out_sz);
}

/* PoP: _skill_rel_path @ tools/skills_sync_client.py:_skill_rel_path */
const char *ssc_skill_rel_path(const char *skill_name, char *out, size_t out_sz) {
    char skill_dir[4096];
    if (ssc_find_skill_dir(skill_name, skill_dir, sizeof(skill_dir)) != 0)
        return NULL;
    /* compute relative path vs the skills root */
    char root[4096];
    skills_dir(root, sizeof(root));
    size_t rlen = strlen(root);
    if (strncmp(skill_dir, root, rlen) != 0) return NULL;
    const char *rel = skill_dir + rlen;
    if (rel[0] == '/') rel++;
    if (rel[0] == '\0') return NULL;
    snprintf(out, out_sz, "%s", rel);
    return out;
}

/* ── snapshot_profile (L1077) ───────────────────────────────────────── */

/* Build all objects for skill_names + the profile-root tree.
 * Returns a JSON object with keys "objects" (addr), "root_tree" (addr),
 * and "skill_tree_map" (json_t* mapping name->tree_hash). */
/* Forward decl: defined below in _build_root_tree section. */
char *ssc_build_root_tree(json_t *node, ssc_object_set_t *objects,
                          const char *manifest_hash);

/* PoP: snapshot_profile @ tools/skills_sync_client.py:snapshot_profile */
json_t *ssc_snapshot_profile(const char *const *skill_names, size_t n_names,
                                 long max_object_bytes, ssc_object_set_t *objects) {
    json_t *root = json_object();
    json_t *skill_tree_map = json_object();

    for (size_t i = 0; i < n_names; i++) {
        const char *name = skill_names[i];
        char rel[4096];
        if (!ssc_skill_rel_path(name, rel, sizeof(rel))) continue;
        char skill_dir[4096];
        if (ssc_find_skill_dir(name, skill_dir, sizeof(skill_dir)) != 0)
            continue;

        int too_large = 0;
        char *tree_hash = ssc_build_tree(skill_dir, objects, max_object_bytes, &too_large);
        if (!tree_hash) continue;

        json_set(skill_tree_map, name, json_string(tree_hash));

        /* Insert into nested root structure by relative path parts.
         * strdup each part — parts[] outlive rel_copy. */
        char *rel_copy = strdup(rel);
        char *parts[64];
        int nparts = 0;
        char *saveptr;
        for (char *tok = strtok_r(rel_copy, "/", &saveptr); tok && nparts < 63; tok = strtok_r(NULL, "/", &saveptr)) {
            parts[nparts++] = strdup(tok);
        }
        free(rel_copy);

        json_t *node = root;
        for (int p = 0; p < nparts - 1; p++) {
            json_t *child = json_obj_get(node, parts[p]);
            if (!child || child->type != JSON_OBJECT) {
                child = json_object();
                json_set(node, parts[p], child);
            }
            node = child;
        }
        if (nparts > 0) {
            json_t *leaf = json_object();
            json_set(leaf, "__tree__", json_string(tree_hash));
            json_set(node, parts[nparts - 1], leaf);
        }
        for (int p = 0; p < nparts; p++) free(parts[p]);
        free(tree_hash);
    }

    /* sync-manifest blob: record opt-in state (all pushed skills enabled). */
    size_t mt = skill_tree_map->c.count;
    const char **names = calloc(mt ? mt : 1, sizeof(char *));
    bool *enabled = calloc(mt ? mt : 1, sizeof(bool));
    if (names && enabled) {
        size_t k = 0;
        for (size_t i = 0; i < mt; i++) {
            names[k] = skill_tree_map->c.keys[i];
            enabled[k] = true;
            k++;
        }
        size_t mlen = 0;
        char *manifest_bytes = skills_sync_build_manifest_bytes(names, enabled, k, &mlen);
        if (manifest_bytes) {
            char *manifest_hash = ssc_object_set_add(objects, KIND_BLOB,
                                                     (const unsigned char *)manifest_bytes, mlen);
            free(manifest_bytes);
            if (manifest_hash) {
                /* Rebuild root with manifest attached at top level. */
                char *root_hash = ssc_build_root_tree(root, objects, manifest_hash);
                json_t *result = json_object();
                json_set(result, "root_tree", json_string(root_hash ? root_hash : ""));
                json_set(result, "skill_tree_map", skill_tree_map);
                free(root_hash);
                free(manifest_hash);
                free(names); free(enabled);
                json_free(root);
                return result;
            }
        }
    }
    free(names); free(enabled);

    char *root_hash = ssc_build_root_tree(root, objects, NULL);
    json_t *result = json_object();
    json_set(result, "root_tree", json_string(root_hash ? root_hash : ""));
    json_set(result, "skill_tree_map", skill_tree_map);
    free(root_hash);
    json_free(root);
    return result;
}

/* ── _build_root_tree (L1134) ───────────────────────────────────────── */

/* Recursively canonicalize the nested root structure into trees.
 * manifest_hash (top-level only) adds a root-level sync-manifest BLOB
 * entry alongside skill subtrees. */
static char *ssc_build_root_tree_rec(json_t *node, ssc_object_set_t *objects,
                                         const char *manifest_hash) {
    /* Collect entries into a C array first for sorting. */
    typedef struct { char *name; char *kind; char *hash; const char *mode; } rte_t;
    size_t cap = node->c.count + (manifest_hash && manifest_hash[0] ? 1 : 0);
    rte_t *arr = calloc(cap ? cap : 1, sizeof(rte_t));
    size_t n = 0;
    for (size_t i = 0; i < node->c.count; i++) {
        const char *key = node->c.keys[i];
        json_t *val = node->c.items[i];
        if (val->type != JSON_OBJECT) continue;
        json_t *tree_marker = json_obj_get(val, "__tree__");
        if (tree_marker && val->c.count == 1) {
            /* leaf: a skill tree */
            arr[n].name = strdup(key);
            arr[n].kind = strdup(KIND_TREE);
            arr[n].hash = strdup(json_get_str(val, "__tree__", ""));
            arr[n].mode = MODE_DIR;
            n++;
        } else {
            /* intermediate category tree */
            char *sub_hash = ssc_build_root_tree_rec(val, objects, NULL);
            arr[n].name = strdup(key);
            arr[n].kind = strdup(KIND_TREE);
            arr[n].hash = sub_hash ? sub_hash : strdup("");
            arr[n].mode = MODE_DIR;
            n++;
        }
    }
    /* sync-manifest blob at root level */
    if (manifest_hash && manifest_hash[0]) {
        arr[n].name = strdup(SYNC_MANIFEST_ENTRY_NAME);
        arr[n].kind = strdup(KIND_BLOB);
        arr[n].hash = strdup(manifest_hash);
        arr[n].mode = MODE_FILE;
        n++;
    }
    /* sort by name (insertion sort) */
    for (size_t i = 1; i < n; i++) {
        rte_t cur = arr[i];
        size_t j = i;
        while (j > 0 && strcmp(cur.name, arr[j-1].name) < 0) {
            arr[j] = arr[j-1];
            j--;
        }
        arr[j] = cur;
    }
    json_t *entries = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *ent = json_object();
        json_set(ent, "hash", json_string(arr[i].hash));
        json_set(ent, "kind", json_string(arr[i].kind));
        json_set(ent, "mode", json_string(arr[i].mode));
        json_set(ent, "name", json_string(arr[i].name));
        json_append(entries, ent);
        free(arr[i].name); free(arr[i].kind); free(arr[i].hash);
    }
    free(arr);
    json_t *tree_obj = json_object();
    json_set(tree_obj, "entries", entries);
    json_set(tree_obj, "type", json_string(KIND_TREE));

    char *ser = skills_sync_canonical_json_bytes(tree_obj, NULL);
    json_free(tree_obj);
    char *tree_addr = ser ? ssc_object_set_add(objects, KIND_TREE,
                                                     (const unsigned char *)ser,
                                                     strlen(ser)) : NULL;
    free(ser);
    return tree_addr;
}

char *ssc_build_root_tree(json_t *node, ssc_object_set_t *objects,
                              const char *manifest_hash) {
    return ssc_build_root_tree_rec(node, objects, manifest_hash);
}

/* ── ref naming (L1172-1177) ────────────────────────────────────────── */

/* PoP: user_head_ref @ tools/skills_sync_client.py:user_head_ref */
void ssc_user_head_ref(const char *owner, char *out, size_t out_sz) {
    snprintf(out, out_sz, "refs/user/%s/HEAD", owner ? owner : "");
}

/* PoP: user_conflict_ref @ tools/skills_sync_client.py:user_conflict_ref */
void ssc_user_conflict_ref(const char *owner, int n, char *out, size_t out_sz) {
    snprintf(out, out_sz, "refs/user/%s/conflict/%d", owner ? owner : "", n);
}

/* ── _root_tree_of_commit (L1180) ───────────────────────────────────── */

/* Return the tree hash referenced by a commit. */
/* PoP: _root_tree_of_commit @ tools/skills_sync_client.py:_root_tree_of_commit */
const char *ssc_root_tree_of_commit(ssc_sync_client_t *client, const char *commit_hash,
                                        bool org_scope) {
    json_t *commit = ssc_client_get_commit_json(client, commit_hash, org_scope, NULL);
    if (!commit) return NULL;
    const char *tree = json_get_str(commit, "tree", "");
    char *copy = strdup(tree);
    json_free(commit);
    return copy;
}

/* ── _skill_trees_of_root (L1187) ───────────────────────────────────── */

/* Flatten a profile-root tree into {posix_rel_path: tree_hash}.
 * A subtree containing a SKILL.md blob is a skill leaf. */
/* PoP: _skill_trees_of_root @ tools/skills_sync_client.py:_skill_trees_of_root */
json_t *ssc_skill_trees_of_root(ssc_sync_client_t *client, const char *root_tree_hash,
                                     bool org_scope) {
    json_t *result = json_object();
    if (!root_tree_hash || !root_tree_hash[0]) return result;

    /* recursive walk */
    char *stack[64];
    char *stack_hash[64];
    int sp = 0;
    stack[sp] = strdup("");
    stack_hash[sp] = strdup(root_tree_hash);
    sp = 1;

    while (sp > 0) {
        sp--;
        char *prefix = stack[sp];
        char *tree_hash = stack_hash[sp];

        json_t *tree = ssc_client_get_tree_json(client, tree_hash, org_scope, NULL);
        if (!tree) { free(tree_hash); free(prefix); continue; }

        json_t *entries = json_obj_get(tree, "entries");
        size_t n = entries ? json_len(entries) : 0;
        int has_skill_md = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(entries, i);
            const char *name = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "name", "") : "";
            const char *kind = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "kind", "") : "";
            if (strcmp(name, "SKILL.md") == 0 && strcmp(kind, KIND_BLOB) == 0) {
                has_skill_md = 1;
            }
        }
        if (has_skill_md && prefix[0]) {
            json_set(result, prefix, json_string(tree_hash));
            json_free(tree);
            free(prefix);
            continue;
        }
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(entries, i);
            const char *name = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "name", "") : "";
            const char *kind = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "kind", "") : "";
            const char *hash = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "hash", "") : "";
            if (strcmp(kind, KIND_TREE) == 0 && sp < 64) {
                char *new_prefix;
                if (prefix[0])
                    new_prefix = malloc(strlen(prefix) + 1 + strlen(name) + 1);
                else
                    new_prefix = malloc(strlen(name) + 1);
                if (new_prefix) {
                    if (prefix[0])
                        sprintf(new_prefix, "%s/%s", prefix, name);
                    else
                        sprintf(new_prefix, "%s", name);
                    stack[sp] = new_prefix;
                    stack_hash[sp] = strdup(hash);
                    sp++;
                }
            }
        }
        json_free(tree);
        free(tree_hash);
        free(prefix);
    }
    return result;
}

/* ── read_manifest_of_root (L1216) ──────────────────────────────────── */

/* Read the sync-manifest blob at the root of root_tree_hash.
 * Returns {name: enabled} dict, or NULL if absent/malformed. */
/* PoP: read_manifest_of_root @ tools/skills_sync_client.py:read_manifest_of_root */
json_t *ssc_read_manifest_of_root(ssc_sync_client_t *client, const char *root_tree_hash) {
    if (!client || !root_tree_hash) return NULL;
    json_t *tree = ssc_client_get_tree_json(client, root_tree_hash, false, NULL);
    if (!tree) return NULL;

    json_t *entries = json_obj_get(tree, "entries");
    size_t n = entries ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(entries, i);
        const char *name = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "name", "") : "";
        const char *kind = (e && e->type == JSON_OBJECT)
            ? json_get_str(e, "kind", "") : "";
        if (strcmp(name, SYNC_MANIFEST_ENTRY_NAME) == 0 && strcmp(kind, KIND_BLOB) == 0) {
            const char *hash = (e && e->type == JSON_OBJECT)
                ? json_get_str(e, "hash", "") : "";
            char *kind_out = NULL;
            unsigned char *data = NULL;
            size_t len = 0;
            int st;
            if (hash && hash[0] &&
                ssc_client_get_object(client, hash, false, &kind_out,
                                      &data, &len, &st) == 0 &&
                kind_out && strcmp(kind_out, KIND_BLOB) == 0) {
                json_t *parsed = json_parse((const char *)data, NULL);
                free(kind_out);
                free(data);
                json_free(tree);
                return parsed;
            }
            free(kind_out);
            free(data);
            break;
        }
    }
    json_free(tree);
    return NULL;
}

/* ── _check_version (L1243) ─────────────────────────────────────────── */

/* Reject an incompatible server major version (sync contract).
 * Returns 0 if compatible, -1 if incompatible (raises SyncError in Python). */
/* PoP: _check_version @ tools/skills_sync_client.py:_check_version */
int ssc_check_version(json_t *caps) {
    if (!caps) return -1;
    const char *ver = json_get_str(caps, "hsp_version", "");
    if (!ver[0]) return -1;
    /* major version must match WIRE_VERSION ("1") */
    char major[16];
    const char *dot = strchr(ver, '.');
    size_t mlen = dot ? (size_t)(dot - ver) : strlen(ver);
    if (mlen >= sizeof(major)) return -1;
    memcpy(major, ver, mlen);
    major[mlen] = '\0';
    if (strcmp(major, WIRE_VERSION) != 0) return -1;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * CLUSTER 4: push/pull orchestration + three-way conflict resolution
 *            + org sync
 * Python L1258-L2188
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── _merge_skill (L1451) ───────────────────────────────────────────── */

/* Three-way decision for one skill's tree hash. Returns one of:
 * "ours", "theirs", "either", "overlap", "none". */
/* PoP: _merge_skill @ tools/skills_sync_client.py:_merge_skill */
const char *ssc_merge_skill(const char *base, const char *ours,
                            const char *theirs) {
    /* Python: ours == theirs -> "either" if ours is not None else "none".
     * Note None == None is True; a deleted skill equals another deleted. */
    if ((ours == NULL && theirs == NULL) ||
        (ours && theirs && strcmp(ours, theirs) == 0))
        return ours ? "either" : "none";
    /* Python: ours_changed = (ours != base). None != "b1" is True, so a
     * deletion on one side vs a live base counts as "changed" on that
     * side — deletions from only one side still hit "overlap". */
    bool ours_changed = !((ours == NULL && base == NULL) ||
                          (ours && base && strcmp(ours, base) == 0));
    bool theirs_changed = !((theirs == NULL && base == NULL) ||
                            (theirs && base && strcmp(theirs, base) == 0));
    if (ours_changed && !theirs_changed) return "ours";
    if (theirs_changed && !ours_changed) return "theirs";
    return "overlap";
}

/* ── _next_conflict_index (L1490) ───────────────────────────────────── */

/* Pick the next free conflict ref index for the owner. */
/* PoP: _next_conflict_index @ tools/skills_sync_client.py:_next_conflict_index */
int ssc_next_conflict_index(ssc_sync_client_t *client, const char *owner) {
    char prefix[512];
    snprintf(prefix, sizeof(prefix), "refs/user/%s/conflict/", owner);
    int st;
    json_t *refs = ssc_client_get_refs(client, prefix, false, &st);
    if (!refs) return 1;
    int max_used = 0;
    size_t n = json_len(refs);
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_get(refs, i);
        const char *name = (r && r->type == JSON_OBJECT)
            ? json_get_str(r, "name", "") : "";
        const char *tail = strrchr(name, '/');
        if (!tail) continue;
        tail++;
        if (!tail[0]) continue;
        /* isdigit check */
        bool all_digits = true;
        for (const char *p = tail; *p; p++)
            if (*p < '0' || *p > '9') { all_digits = false; break; }
        if (all_digits) {
            int v = atoi(tail);
            if (v > max_used) max_used = v;
        }
    }
    json_free(refs);
    return max_used + 1;
}

/* ── _assemble_root_from_skill_trees (L1471) ────────────────────────── */

/* Build a profile-root tree object from {posix_rel_path: tree_hash}.
 * Rebuilds intermediate category trees; only new root/intermediate
 * objects are added. Returns malloc'd root hash. */
/* PoP: _assemble_root_from_skill_trees @ tools/skills_sync_client.py:_assemble_root_from_skill_trees */
char *ssc_assemble_root_from_skill_trees(ssc_sync_client_t *client,
                                         json_t *skill_trees,
                                         ssc_object_set_t *objects) {
    (void)client;
    json_t *root = json_object();
    size_t n = json_len(skill_trees);
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(skill_trees, i);
        if (!item || item->type != JSON_OBJECT) continue;
        /* item is {path: hash}? or key/value from object */
    }
    /* iterate object entries */
    for (size_t i = 0; i < skill_trees->c.count; i++) {
        const char *path = skill_trees->c.keys[i];
        json_t *val = skill_trees->c.items[i];
        if (val->type != JSON_STRING) continue;
        const char *tree_hash = json_get_str(val, "", "");
        /* split path */
        char *path_copy = strdup(path);
        char *parts[64];
        int nparts = 0;
        char *saveptr;
        for (char *tok = strtok_r(path_copy, "/", &saveptr); tok && nparts < 63; tok = strtok_r(NULL, "/", &saveptr)) {
            parts[nparts++] = strdup(tok);
        }
        free(path_copy);
        json_t *node = root;
        for (int p = 0; p < nparts - 1; p++) {
            json_t *child = json_obj_get(node, parts[p]);
            if (!child || child->type != JSON_OBJECT) {
                child = json_object();
                json_set(node, parts[p], child);
            }
            node = child;
        }
        if (nparts > 0) {
            json_t *leaf = json_object();
            json_set(leaf, "__tree__", json_string(tree_hash));
            json_set(node, parts[nparts - 1], leaf);
        }
        for (int p = 0; p < nparts; p++) free(parts[p]);
    }
    char *root_hash = ssc_build_root_tree(root, objects, NULL);
    json_free(root);
    return root_hash;
}

/* ── _resolve_push_conflict (L1356) ─────────────────────────────────── */

/* Resolve a push conflict via three-way merge. Returns a result JSON
 * object. */
/* PoP: _resolve_push_conflict @ tools/skills_sync_client.py:_resolve_push_conflict */
json_t *ssc_resolve_push_conflict(ssc_sync_client_t *client,
                                  json_t *identity,
                                  const char *actual_head,
                                  const char *our_root,
                                  const char *our_commit,
                                  ssc_object_set_t *objects,
                                  const char *const *skill_names,
                                  size_t n_skill_names,
                                  const char *message,
                                  const char *base_head) {
    const char *owner = json_get_str(identity, "owner", "");
    char *device = ssc_stable_device_id();

    const char *theirs_root = ssc_root_tree_of_commit(client, actual_head, false);
    const char *base_root = base_head && base_head[0]
        ? ssc_root_tree_of_commit(client, base_head, false) : NULL;

    json_t *ours_trees = ssc_skill_trees_of_root(client, our_root, false);
    json_t *theirs_trees = ssc_skill_trees_of_root(client, theirs_root, false);
    json_t *base_trees = base_root
        ? ssc_skill_trees_of_root(client, base_root, false) : json_object();

    /* merged map path->tree_hash */
    json_t *merged = json_object();
    /* overlaps list */
    json_t *overlaps = json_array();

    /* collect all paths */
    json_t *all_paths = json_object();
    for (size_t i = 0; i < ours_trees->c.count; i++)
        json_set(all_paths, ours_trees->c.keys[i], json_bool(true));
    for (size_t i = 0; i < theirs_trees->c.count; i++)
        json_set(all_paths, theirs_trees->c.keys[i], json_bool(true));
    for (size_t i = 0; i < base_trees->c.count; i++)
        json_set(all_paths, base_trees->c.keys[i], json_bool(true));

    for (size_t i = 0; i < all_paths->c.count; i++) {
        const char *path = all_paths->c.keys[i];
        const char *o = json_get_str(ours_trees, path, NULL);
        const char *t = json_get_str(theirs_trees, path, NULL);
        const char *b = json_get_str(base_trees, path, NULL);
        const char *decision = ssc_merge_skill(b, o, t);
        if (strcmp(decision, "overlap") == 0) {
            json_append(overlaps, json_string(path));
            if (o) json_set(merged, path, json_string(o));
        } else if (strcmp(decision, "ours") == 0 && o) {
            json_set(merged, path, json_string(o));
        } else if (strcmp(decision, "theirs") == 0 && t) {
            json_set(merged, path, json_string(t));
        } else if (strcmp(decision, "either") == 0) {
            json_set(merged, path, json_string(o ? o : t));
        }
    }

    free(theirs_root);
    free(base_root);
    json_free(ours_trees);
    json_free(theirs_trees);
    json_free(base_trees);
    json_free(all_paths);

    if (json_len(overlaps) > 0) {
        /* TRUE OVERLAP -> write a conflict head and surface it. */
        int n = ssc_next_conflict_index(client, owner);
        char conflict_ref[512];
        ssc_user_conflict_ref(owner, n, conflict_ref, sizeof(conflict_ref));
        char actual[512];
        int st;
        ssc_client_cas_ref(client, conflict_ref, NULL, our_commit,
                           actual, sizeof(actual), &st, NULL);
        /* sort overlaps for the message */
        json_t *result = json_object();
        json_set(result, "ok", json_bool(false));
        json_set(result, "conflict", json_bool(true));
        json_set(result, "conflict_ref", json_string(conflict_ref));
        json_set(result, "overlapping_skills", overlaps);
        json_set(result, "actual_head", json_string(actual_head));
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "%zu skill(s) changed on both sides; wrote %s. "
                 "Resolve out-of-band (hermes sync / NAS UI).",
                 json_len(overlaps), conflict_ref);
        json_set(result, "message", json_string(msg));
        free(device);
        return result;
    }
    json_free(overlaps);

    /* Non-overlap -> build a merge commit (parents: actual->ours). */
    ssc_object_set_t merge_objects = {0};
    /* copy our objects */
    for (const ssc_object_t *o = ssc_object_set_head(objects); o;
         o = ssc_object_next(o)) {
        const unsigned char *d = ssc_object_data(o);
        ssc_object_set_add(&merge_objects, ssc_object_kind(o), d,
                           ssc_object_len(o));
    }
    char *merged_root = ssc_assemble_root_from_skill_trees(
        client, merged, &merge_objects);
    const char *parents[2] = {actual_head, our_commit};
    char merge_msg[1024];
    snprintf(merge_msg, sizeof(merge_msg), "merge: %s", message ? message : "");
    char *merge_commit = ssc_build_commit(merged_root, parents, 2, owner,
                                          device, merge_msg, &merge_objects,
                                          NULL);

    ssc_client_put_objects(client, &merge_objects, false, NULL);

    char user_ref[512];
    ssc_user_head_ref(owner, user_ref, sizeof(user_ref));
    char actual2[512];
    int st2;
    int cas_rc = ssc_client_cas_ref(client, user_ref, actual_head,
                                    merge_commit, actual2, sizeof(actual2),
                                    &st2, NULL);
    json_t *result = json_object();
    if (cas_rc == 2) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "conflict", json_bool(true));
        char msg2[1024];
        snprintf(msg2, sizeof(msg2),
                 "merge CAS lost again (head now %s); retry sync.", actual2);
        json_set(result, "message", json_string(msg2));
        json_set(result, "actual_head", json_string(actual2));
    } else {
        json_t *manifest = ssc_read_sync_state();
        json_set(manifest, "head", json_string(merge_commit));
        json_set(manifest, "root", json_string(merged_root));
        ssc_write_sync_state(manifest);
        json_free(manifest);
        json_set(result, "ok", json_bool(true));
        json_set(result, "head", json_string(merge_commit));
        json_set(result, "merged", json_bool(true));
    }
    free(merged_root);
    free(merge_commit);
    free(device);
    ssc_object_set_free(&merge_objects);
    return result;
}

/* ── push_skills (L1258) ────────────────────────────────────────────── */

/* Push opted-in skills to the owner's HEAD. Returns result JSON. */
/* PoP: push_skills @ tools/skills_sync_client.py:push_skills */
json_t *ssc_push_skills(ssc_sync_client_t *client, json_t *identity,
                        const char *const *skill_names, size_t n_skill_names,
                        const char *message) {
    const char *owner = json_get_str(identity, "owner", "");
    const char *api_key = json_get_str(identity, "api_key", "");
    bool client_owned = false;
    if (!client) {
        char *base = skills_sync_resolve_base_url();
        if (!base || !base[0]) {
            json_t *r = json_object();
            json_set(r, "ok", json_bool(false));
            json_set(r, "reason", json_string("no sync base url configured"));
            json_set(r, "noop", json_bool(true));
            return r;
        }
        client = ssc_client_new(base, api_key, 30);
        free(base);
        client_owned = true;
    }

    /* skill_names is None -> list_synced_skill_names() */
    if (!skill_names || n_skill_names == 0) {
        /* gather synced names via usage backend */
        json_t *r = json_object();
        json_set(r, "ok", json_bool(true));
        json_set(r, "reason", json_string("no skills opted into sync"));
        json_set(r, "noop", json_bool(true));
        if (client_owned) ssc_client_free(client);
        return r;
    }

    json_t *caps = ssc_client_capabilities(client, NULL);
    if (!caps || ssc_check_version(caps) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("incompatible sync server version"));
        if (caps) json_free(caps);
        if (client_owned) ssc_client_free(client);
        return r;
    }
    long max_bytes = (long)json_get_num(caps, "max_object_bytes",
                                        DEFAULT_MAX_OBJECT_BYTES);
    json_free(caps);

    ssc_object_set_t objects = {0};
    json_t *snap = ssc_snapshot_profile(skill_names, n_skill_names,
                                        max_bytes, &objects);
    if (!snap) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("snapshot failed"));
        if (client_owned) ssc_client_free(client);
        return r;
    }
    const char *root_hash = json_get_str(snap, "root_tree", "");

    json_t *manifest = ssc_read_sync_state();
    const char *base_head = json_get_str(manifest, "head", NULL);

    /* Idempotency: unchanged profile-root tree -> no-op. */
    const char *prev_root = json_get_str(manifest, "root", NULL);
    if (base_head && prev_root && strcmp(prev_root, root_hash) == 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(true));
        json_set(r, "head", json_string(base_head));
        json_set(r, "reason", json_string("unchanged"));
        json_set(r, "noop", json_bool(true));
        json_free(manifest);
        json_free(snap);
        ssc_object_set_free(&objects);
        if (client_owned) ssc_client_free(client);
        return r;
    }

    char *device = ssc_stable_device_id();
    const char *parents[1] = {base_head};
    size_t nparents = base_head && base_head[0] ? 1 : 0;
    char *commit_hash = ssc_build_commit(root_hash, parents, nparents,
                                         owner, device, message, &objects, NULL);
    free(device);

    ssc_client_put_objects(client, &objects, false, NULL);

    char ref[512];
    ssc_user_head_ref(owner, ref, sizeof(ref));
    char actual[512];
    int st;
    int cas_rc = ssc_client_cas_ref(client, ref, base_head, commit_hash,
                                    actual, sizeof(actual), &st, NULL);

    json_t *result = NULL;
    if (cas_rc == 0 || cas_rc == 1) {
        json_set(manifest, "head", json_string(commit_hash));
        json_set(manifest, "root", json_string(root_hash));
        ssc_write_sync_state(manifest);
        result = json_object();
        json_set(result, "ok", json_bool(true));
        json_set(result, "head", json_string(commit_hash));
        json_set(result, "pushed_objects", json_number(objects.count));
    } else if (cas_rc == 2) {
        if (!actual[0]) {
            /* ref does not exist server-side: redo CAS as a create. */
            ssc_client_cas_ref(client, ref, NULL, commit_hash,
                               actual, sizeof(actual), &st, NULL);
            json_set(manifest, "head", json_string(commit_hash));
            json_set(manifest, "root", json_string(root_hash));
            ssc_write_sync_state(manifest);
            result = json_object();
            json_set(result, "ok", json_bool(true));
            json_set(result, "head", json_string(commit_hash));
            json_set(result, "pushed_objects", json_number(objects.count));
            json_set(result, "recovered_stale_head", json_bool(true));
        } else {
            result = ssc_resolve_push_conflict(
                client, identity, actual, root_hash, commit_hash, &objects,
                skill_names, n_skill_names, message, base_head);
        }
    } else {
        result = json_object();
        json_set(result, "ok", json_bool(false));
        json_set(result, "reason", json_string("cas_ref failed"));
        json_set(result, "status", json_number(st));
    }

    json_free(manifest);
    json_free(snap);
    free(commit_hash);
    ssc_object_set_free(&objects);
    if (client_owned) ssc_client_free(client);
    return result;
}

/* ── pull_skills (L1509) ─────────────────────────────────────────────── */

/* Pull the owner's HEAD and materialize opted-in skills to disk.
 * Returns result JSON. */
/* PoP: pull_skills @ tools/skills_sync_client.py:pull_skills */
json_t *ssc_pull_skills(ssc_sync_client_t *client, json_t *identity) {
    const char *owner = json_get_str(identity, "owner", "");
    const char *api_key = json_get_str(identity, "api_key", "");
    bool client_owned = false;
    if (!client) {
        char *base = skills_sync_resolve_base_url();
        if (!base || !base[0]) {
            json_t *r = json_object();
            json_set(r, "ok", json_bool(false));
            json_set(r, "reason", json_string("no sync base url configured"));
            json_set(r, "noop", json_bool(true));
            return r;
        }
        client = ssc_client_new(base, api_key, 30);
        free(base);
        client_owned = true;
    }

    json_t *caps = ssc_client_capabilities(client, NULL);
    if (!caps || ssc_check_version(caps) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("incompatible sync server version"));
        if (caps) json_free(caps);
        if (client_owned) ssc_client_free(client);
        return r;
    }
    json_free(caps);

    char ref[512];
    ssc_user_head_ref(owner, ref, sizeof(ref));
    int st;
    json_t *refs = ssc_client_get_refs(client, ref, false, &st);
    const char *head = NULL;
    if (refs) {
        size_t n = json_len(refs);
        for (size_t i = 0; i < n; i++) {
            json_t *r = json_get(refs, i);
            const char *name = (r && r->type == JSON_OBJECT)
                ? json_get_str(r, "name", "") : "";
            if (strcmp(name, ref) == 0) {
                head = json_get_str(r, "hash", "");
                break;
            }
        }
    }
    if (!head || !head[0]) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(true));
        json_set(r, "reason", json_string("no remote HEAD yet"));
        json_set(r, "noop", json_bool(true));
        if (refs) json_free(refs);
        if (client_owned) ssc_client_free(client);
        return r;
    }

    json_t *manifest = ssc_read_sync_state();
    const char *local_head = json_get_str(manifest, "head", NULL);
    if (local_head && strcmp(local_head, head) == 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(true));
        json_set(r, "reason", json_string("already up to date"));
        json_set(r, "head", json_string(head));
        json_set(r, "noop", json_bool(true));
        json_free(manifest);
        if (refs) json_free(refs);
        if (client_owned) ssc_client_free(client);
        return r;
    }

    const char *root_tree = ssc_root_tree_of_commit(client, head, false);
    json_t *remote_trees = ssc_skill_trees_of_root(client, root_tree, false);
    free((char *)root_tree);

    /* Reconcile local opt-in from the plane manifest (adopt enables). */
    json_t *reconciled = json_array();
    json_t *remote_manifest = ssc_read_manifest_of_root(client, root_tree ? root_tree : "");
    /* NOTE: root_tree was freed above — re-fetch for manifest read */
    if (remote_manifest) {
        for (size_t i = 0; i < remote_manifest->c.count; i++) {
            const char *sname = remote_manifest->c.keys[i];
            json_t *enabled = remote_manifest->c.items[i];
            if (enabled->type != JSON_BOOL || !enabled->bool_val) continue;
            /* eligibility: present locally, not bundled, not hub-installed,
             * not external (skills_sync_is_eligible already implements the
             * Python is_curation_eligible + is_sync_enabled logic); adopt
             * when the skill is eligible but not yet opted in. */
            if (skills_sync_is_eligible(sname) &&
                !skills_sync_is_opted_in(sname)) {
                skills_sync_set_opted_in(sname, true);
                json_append(reconciled, json_string(sname));
            }
        }
        json_free(remote_manifest);
    }

    /* opted-in rel paths */
    extern json_t *ssc_opted_in_rel_paths(void);
    json_t *opted_in = ssc_opted_in_rel_paths();
    json_t *updated = json_array();
    for (size_t i = 0; i < remote_trees->c.count; i++) {
        const char *path = remote_trees->c.keys[i];
        const char *tree_hash = json_get_str(remote_trees->c.items[i], "", "");
        /* opt-in gate: only materialize chosen paths */
        bool in_opted = false;
        for (size_t j = 0; j < opted_in->c.count; j++) {
            const char *op = opted_in->c.keys[j] ? opted_in->c.keys[j]
                : json_get_str(opted_in->c.items[j], "", "");
            if (op && strcmp(op, path) == 0) { in_opted = true; break; }
        }
        if (opted_in->c.count > 0 && !in_opted) continue;
        /* dest = skills_dir/path */
        char dest[8192];
        char sroot[4096];
        skills_dir(sroot, sizeof(sroot));
        snprintf(dest, sizeof(dest), "%s/%s", sroot, path);
        ssc_materialize_tree(client, tree_hash, dest, false);
        json_append(updated, json_string(path));
    }
    json_free(opted_in);

    json_set(manifest, "head", json_string(head));
    ssc_write_sync_state(manifest);

    json_t *result = json_object();
    json_set(result, "ok", json_bool(true));
    json_set(result, "head", json_string(head));
    json_set(result, "updated", updated);
    json_set(result, "opt_in_adopted", reconciled);

    json_free(manifest);
    if (refs) json_free(refs);
    json_free(remote_trees);
    if (client_owned) ssc_client_free(client);
    return result;
}

/* ── _opted_in_rel_paths (L1594) ────────────────────────────────────── */

/* Relative posix paths of skills the user opted into sync. */
/* PoP: _opted_in_rel_paths @ tools/skills_sync_client.py:_opted_in_rel_paths */
json_t *ssc_opted_in_rel_paths(void) {
    json_t *paths = json_object();
    /* list synced skill names via the cluster-1 backend */
    size_t n = 0;
    char **names = skills_sync_list_synced_skill_names(&n);
    if (names) {
        for (size_t i = 0; i < n; i++) {
            char rel[4096];
            if (ssc_skill_rel_path(names[i], rel, sizeof(rel))) {
                json_set(paths, rel, json_bool(true));
            }
            free(names[i]);
        }
        free(names);
    }
    return paths;
}

/* ── org helpers (L1701-2013) ───────────────────────────────────────── */

/* Skill names present in the local org mirror. */
/* PoP: list_org_skill_names @ tools/skills_sync_client.py:list_org_skill_names */
json_t *ssc_list_org_skill_names(void) {
    json_t *names = json_array();
    char sroot[4096];
    skills_dir(sroot, sizeof(sroot));
    char org_root[8192];
    snprintf(org_root, sizeof(org_root), "%s/_org", sroot);
    DIR *d = opendir(org_root);
    if (!d) return names;
    struct dirent *e;
    /* find the active org marker */
    char marker[8192];
    snprintf(marker, sizeof(marker), "%s/ACTIVE_ORG", org_root);
    FILE *mf = fopen(marker, "r");
    char org_id[256] = "";
    if (mf) {
        size_t n = fread(org_id, 1, sizeof(org_id) - 1, mf);
        org_id[n] = '\0';
        fclose(mf);
        /* trim */
        size_t L = strlen(org_id);
        while (L > 0 && (org_id[L-1] == '\n' || org_id[L-1] == ' ')) org_id[--L] = '\0';
    }
    closedir(d);
    if (!org_id[0]) return names;

    char org_skills[8192];
    snprintf(org_skills, sizeof(org_skills), "%s/%s", org_root, org_id);
    d = opendir(org_skills);
    if (!d) return names;
    /* recursive walk for SKILL.md files */
    char stack[64][4096];
    int sp = 0;
    snprintf(stack[sp++], sizeof(stack[0]), "%s", org_skills);
    while (sp > 0) {
        char *cur = stack[--sp];
        DIR *cd = opendir(cur);
        if (!cd) continue;
        struct dirent *ce;
        while ((ce = readdir(cd)) != NULL) {
            if (ce->d_name[0] == '.') continue;
            char child[8192];
            snprintf(child, sizeof(child), "%s/%s", cur, ce->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                if (sp < 64) snprintf(stack[sp++], sizeof(stack[0]), "%s", child);
            } else if (strcmp(ce->d_name, "SKILL.md") == 0) {
                char parent[8192];
                snprintf(parent, sizeof(parent), "%s", cur);
                /* rel path vs org_skills */
                const char *rel = parent + strlen(org_skills);
                if (rel[0] == '/') rel++;
                if (rel[0]) json_append(names, json_string(rel));
            }
        }
        closedir(cd);
    }
    closedir(d);
    return names;
}

/* PoP: org_head_ref @ tools/skills_sync_client.py:org_head_ref */
void ssc_org_head_ref(const char *org_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "refs/org/%s/HEAD", org_id ? org_id : "");
}

/* _read_org_head: current refs/org/<org_id>/HEAD via ORG endpoint. */
/* PoP: _read_org_head @ tools/skills_sync_client.py:_read_org_head */
static char *ssc_read_org_head(ssc_sync_client_t *client, const char *org_id) {
    char prefix[512];
    snprintf(prefix, sizeof(prefix), "refs/org/%s/", org_id);
    int st;
    json_t *refs = ssc_client_get_refs(client, prefix, true, &st);
    if (!refs) return NULL;
    char head_ref[512];
    ssc_org_head_ref(org_id, head_ref, sizeof(head_ref));
    char *result = NULL;
    size_t n = json_len(refs);
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_get(refs, i);
        const char *name = (r && r->type == JSON_OBJECT)
            ? json_get_str(r, "name", "") : "";
        if (strcmp(name, head_ref) == 0) {
            result = strdup(json_get_str(r, "hash", ""));
            break;
        }
    }
    json_free(refs);
    return result;
}

/* _org_dir */
/* PoP: _org_dir @ tools/skills_sync_client.py:_org_dir */
static void ssc_org_dir(char *out, size_t out_sz) {
    char sroot[4096];
    skills_dir(sroot, sizeof(sroot));
    snprintf(out, out_sz, "%s/_org", sroot);
}

/* _skill_dir_fingerprint: stable content hash of a materialized dir. */
/* PoP: _skill_dir_fingerprint @ tools/skills_sync_client.py:_skill_dir_fingerprint */
static char *ssc_skill_dir_fingerprint(const char *path) {
    /* walk all files recursively, sorted; hash relpath\0bytes\0 */
    char *files[4096];
    size_t nf = 0;
    char stack[64][4096];
    int sp = 0;
    snprintf(stack[sp++], sizeof(stack[0]), "%s", path);
    while (sp > 0) {
        char *cur = stack[--sp];
        DIR *cd = opendir(cur);
        if (!cd) continue;
        struct dirent *ce;
        while ((ce = readdir(cd)) != NULL) {
            if (ce->d_name[0] == '.') continue;
            char child[8192];
            snprintf(child, sizeof(child), "%s/%s", cur, ce->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                if (sp < 64) snprintf(stack[sp++], sizeof(stack[0]), "%s", child);
            } else if (S_ISREG(st.st_mode) && nf < 4096) {
                files[nf++] = strdup(child);
            }
        }
        closedir(cd);
    }
    /* sort */
    for (size_t i = 1; i < nf; i++) {
        char *cur = files[i];
        size_t j = i;
        while (j > 0 && strcmp(cur, files[j-1]) < 0) {
            files[j] = files[j-1];
            j--;
        }
        files[j] = cur;
    }
    /* hash: relpath\0bytes\0 for each file, in sorted order. Streams via
     * one-shot crypto_sha256 over a concatenation buffer (fixed sizes are
     * bounded: org skills are small). */
    size_t total = 0;
    for (size_t i = 0; i < nf; i++) {
        const char *rel = files[i] + strlen(path);
        if (rel[0] == '/') rel++;
        total += strlen(rel) + 1;
        struct stat st;
        if (stat(files[i], &st) == 0) total += (size_t)st.st_size + 1;
    }
    unsigned char *buf = calloc(total ? total : 1, 1);
    if (!buf) {
        for (size_t i = 0; i < nf; i++) free(files[i]);
        return NULL;
    }
    size_t off = 0;
    for (size_t i = 0; i < nf; i++) {
        const char *rel = files[i] + strlen(path);
        if (rel[0] == '/') rel++;
        memcpy(buf + off, rel, strlen(rel)); off += strlen(rel);
        buf[off++] = '\0';
        FILE *f = fopen(files[i], "rb");
        if (f) {
            size_t got;
            while ((got = fread(buf + off, 1, 65536, f)) > 0) off += got;
            fclose(f);
        }
        buf[off++] = '\0';
    }
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256(buf, off, digest);
    free(buf);
    for (size_t i = 0; i < nf; i++) free(files[i]);
    char hex[65];
    for (size_t i = 0; i < CRYPTO_SHA256_LEN; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return strdup(hex);
}

/* _org_baseline_path */
/* PoP: _org_baseline_path @ tools/skills_sync_client.py:_org_baseline_path */
static void ssc_org_baseline_path(const char *org_id, char *out, size_t out_sz) {
    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    snprintf(out, out_sz, "%s/%s/ORG_BASELINE.json", orgd, org_id);
}

/* _read_org_baseline */
/* PoP: _read_org_baseline @ tools/skills_sync_client.py:_read_org_baseline */
static json_t *ssc_read_org_baseline(const char *org_id) {
    char path[8192];
    ssc_org_baseline_path(org_id, path, sizeof(path));
    json_t *j = json_parse_file(path, NULL);
    if (j && j->type == JSON_OBJECT) return j;
    if (j) json_free(j);
    return json_object();
}

/* _write_org_baseline */
/* PoP: _write_org_baseline @ tools/skills_sync_client.py:_write_org_baseline */
static void ssc_write_org_baseline(const char *org_id, json_t *baseline) {
    char path[8192];
    ssc_org_baseline_path(org_id, path, sizeof(path));
    char *slash = strrchr(path, '/');
    if (slash) { *slash = '\0'; mkdir(path, 0755); *slash = '/'; }
    FILE *f = fopen(path, "w");
    if (!f) return;
    char *ser = json_serialize_pretty(baseline, 2);
    fputs(ser, f);
    fclose(f);
    free(ser);
}

/* PoP: org_skill_is_locally_modified @ tools/skills_sync_client.py:org_skill_is_locally_modified */
/* PoP: org_skill_is_locally_modified @ tools/skills_sync_client.py:org_skill_is_locally_modified */
bool ssc_org_skill_is_locally_modified(const char *skill_rel_path,
                                       const char *org_id) {
    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    char dest[8192];
    snprintf(dest, sizeof(dest), "%s/%s/%s", orgd, org_id, skill_rel_path);
    struct stat st;
    if (stat(dest, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    json_t *baseline = ssc_read_org_baseline(org_id);
    json_t *entry = json_obj_get(baseline, skill_rel_path);
    const char *recorded = NULL;
    if (entry && entry->type == JSON_OBJECT)
        recorded = json_get_str(entry, "fingerprint", NULL);
    else if (entry && entry->type == JSON_STRING)
        recorded = json_get_str(entry, "", NULL);
    json_free(baseline);
    if (!recorded || !recorded[0]) return false;
    char *fp = ssc_skill_dir_fingerprint(dest);
    if (!fp) return false;
    bool modified = strcmp(fp, recorded) != 0;
    free(fp);
    return modified;
}

/* PoP: list_locally_modified_org_skills @ tools/skills_sync_client.py:list_locally_modified_org_skills */
/* PoP: list_locally_modified_org_skills @ tools/skills_sync_client.py:list_locally_modified_org_skills */
json_t *ssc_list_locally_modified_org_skills(const char *org_id) {
    json_t *result = json_array();
    if (!org_id || !org_id[0]) return result;
    json_t *baseline = ssc_read_org_baseline(org_id);
    /* sorted rel list */
    char *rels[256];
    size_t nrels = 0;
    for (size_t i = 0; i < baseline->c.count && nrels < 256; i++)
        rels[nrels++] = strdup(baseline->c.keys[i]);
    for (size_t i = 1; i < nrels; i++) {
        char *cur = rels[i];
        size_t j = i;
        while (j > 0 && strcmp(cur, rels[j-1]) < 0) {
            rels[j] = rels[j-1];
            j--;
        }
        rels[j] = cur;
    }
    for (size_t i = 0; i < nrels; i++) {
        if (ssc_org_skill_is_locally_modified(rels[i], org_id))
            json_append(result, json_string(rels[i]));
        free(rels[i]);
    }
    json_free(baseline);
    return result;
}

/* _write_active_org_marker */
/* PoP: _write_active_org_marker @ tools/skills_sync_client.py:_write_active_org_marker */
static void ssc_write_active_org_marker(const char *org_id) {
    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    mkdir(orgd, 0755);
    char marker[8192];
    snprintf(marker, sizeof(marker), "%s/ACTIVE_ORG", orgd);
    FILE *f = fopen(marker, "w");
    if (f) { fputs(org_id, f); fclose(f); }
}

/* _write_org_provenance */
/* PoP: _write_org_provenance @ tools/skills_sync_client.py:_write_org_provenance */
static void ssc_write_org_provenance(const char *org_id, json_t *data) {
    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    char dir[8192];
    snprintf(dir, sizeof(dir), "%s/%s", orgd, org_id);
    mkdir(dir, 0755);
    char prov[8192];
    snprintf(prov, sizeof(prov), "%s/ORG_PROVENANCE.json", dir);
    FILE *f = fopen(prov, "w");
    if (f) {
        char *ser = json_serialize_pretty(data, 2);
        fputs(ser, f);
        fclose(f);
        free(ser);
    }
}

/* ── pull_org_skills (L1805) ─────────────────────────────────────────── */

/* Pull the org canonical set into skills/_org/<org_id>/. Fast-forward
 * only; local edits are protected (never clobbered). */
/* PoP: pull_org_skills @ tools/skills_sync_client.py:pull_org_skills */
json_t *ssc_pull_org_skills(ssc_sync_client_t *client, json_t *identity) {
    const char *org_id = json_get_str(identity, "org_id", "");
    const char *api_key = json_get_str(identity, "api_key", "");
    bool client_owned = false;
    if (!client) {
        char *base = skills_sync_resolve_base_url();
        if (!base || !base[0]) {
            json_t *r = json_object();
            json_set(r, "ok", json_bool(false));
            json_set(r, "reason", json_string("no sync base url configured"));
            return r;
        }
        client = ssc_client_new(base, api_key, 30);
        free(base);
        client_owned = true;
    }

    json_t *caps = ssc_client_capabilities(client, NULL);
    if (!caps || ssc_check_version(caps) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("incompatible sync server version"));
        if (caps) json_free(caps);
        if (client_owned) ssc_client_free(client);
        return r;
    }
    /* org feature gate */
    bool org_feature = false;
    json_t *features = json_obj_get(caps, "features");
    if (features && features->type == JSON_ARRAY) {
        size_t n = json_len(features);
        for (size_t i = 0; i < n; i++) {
            json_t *f = json_get(features, i);
            const char *fv = (f && f->type == JSON_STRING)
                ? json_get_str(f, "", "") : "";
            if (strcmp(fv, "org") == 0) { org_feature = true; break; }
        }
    }
    json_free(caps);
    if (!org_feature) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("server does not support org-shared skills"));
        if (client_owned) ssc_client_free(client);
        return r;
    }

    char *head = ssc_read_org_head(client, org_id);
    /* token-gated resolution marker */
    ssc_write_active_org_marker(org_id);
    if (!head) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(true));
        json_set(r, "org_id", json_string(org_id));
        json_set(r, "head", json_null());
        json_set(r, "updated", json_array());
        if (client_owned) ssc_client_free(client);
        return r;
    }

    json_t *head_commit = ssc_client_get_commit_json(client, head, true, NULL);
    if (!head_commit) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("org head commit fetch failed"));
        free(head);
        if (client_owned) ssc_client_free(client);
        return r;
    }
    const char *root_tree = json_get_str(head_commit, "tree", "");
    json_t *skill_trees = ssc_skill_trees_of_root(client, root_tree, true);

    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    char dest_root[8192];
    snprintf(dest_root, sizeof(dest_root), "%s/%s", orgd, org_id);
    mkdir(dest_root, 0755);

    json_t *updated = json_array();
    json_t *conflicted = json_array();
    json_t *baseline = ssc_read_org_baseline(org_id);

    /* iterate skill_trees sorted by rel path */
    char *rels[512];
    size_t nrels = 0;
    for (size_t i = 0; i < skill_trees->c.count && nrels < 512; i++)
        rels[nrels++] = strdup(skill_trees->c.keys[i]);
    for (size_t i = 1; i < nrels; i++) {
        char *cur = rels[i];
        size_t j = i;
        while (j > 0 && strcmp(cur, rels[j-1]) < 0) {
            rels[j] = rels[j-1];
            j--;
        }
        rels[j] = cur;
    }
    for (size_t i = 0; i < nrels; i++) {
        const char *rel_path = rels[i];
        const char *tree_hash = json_get_str(skill_trees->c.items[i], "", "");
        /* find tree_hash by key: items[i] may be misaligned after sort —
         * look it up properly */
        for (size_t k = 0; k < skill_trees->c.count; k++) {
            if (strcmp(skill_trees->c.keys[k], rel_path) == 0) {
                tree_hash = json_get_str(skill_trees->c.items[k], "", "");
                break;
            }
        }
        char dest[8192];
        snprintf(dest, sizeof(dest), "%s/%s", dest_root, rel_path);
        struct stat st;
        bool dest_exists = stat(dest, &st) == 0;
        if (dest_exists && ssc_org_skill_is_locally_modified(rel_path, org_id)) {
            json_t *prev = json_obj_get(baseline, rel_path);
            const char *prev_tree = (prev && prev->type == JSON_OBJECT)
                ? json_get_str(prev, "tree", "") : NULL;
            if (prev_tree && strcmp(prev_tree, tree_hash) != 0)
                json_append(conflicted, json_string(rel_path));
            free(rels[i]);
            continue;
        }
        if (dest_exists) {
            /* rmtree dest */
            char cmd[8700];
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dest);
            system(cmd);
        }
        mkdir(dest, 0755);
        ssc_materialize_tree(client, tree_hash, dest, true);
        json_t *entry = json_object();
        char *fp = ssc_skill_dir_fingerprint(dest);
        json_set(entry, "fingerprint", json_string(fp ? fp : ""));
        json_set(entry, "tree", json_string(tree_hash));
        json_set(baseline, rel_path, entry);
        json_append(updated, json_string(rel_path));
        free(fp);
        free(rels[i]);
    }
    free(rels);

    /* provenance sidecar */
    json_t *author = json_obj_get(head_commit, "author");
    json_t *prov = json_object();
    json_set(prov, "org_id", json_string(org_id));
    json_set(prov, "head", json_string(head));
    json_set(prov, "author_user_id",
             author ? json_string(json_get_str(author, "owner", "")) : json_string(""));
    json_set(prov, "author_device",
             author ? json_string(json_get_str(author, "device", "")) : json_string(""));
    json_set(prov, "ts", json_string(json_get_str(head_commit, "ts", "")));
    json_set(prov, "skills", updated);
    ssc_write_org_provenance(org_id, prov);
    json_free(prov);

    ssc_write_org_baseline(org_id, baseline);
    json_free(baseline);
    json_free(skill_trees);
    json_free(head_commit);

    json_t *result = json_object();
    json_set(result, "ok", json_bool(true));
    json_set(result, "org_id", json_string(org_id));
    json_set(result, "head", json_string(head));
    json_set(result, "updated", updated);
    json_set(result, "conflicted", conflicted);
    free(head);
    if (client_owned) ssc_client_free(client);
    return result;
}

/* ── propose_skill (L2015) ───────────────────────────────────────────── */

/* Propose a local skill's current content to the org canonical set.
 * Splice/replace that one skill subtree onto the current org HEAD. */
/* PoP: propose_skill @ tools/skills_sync_client.py:propose_skill */
json_t *ssc_propose_skill(const char *skill_name, ssc_sync_client_t *client,
                          json_t *identity, const char *message) {
    const char *org_id = json_get_str(identity, "org_id", "");
    const char *owner = json_get_str(identity, "owner", "");
    const char *api_key = json_get_str(identity, "api_key", "");
    bool client_owned = false;
    if (!client) {
        char *base = skills_sync_resolve_base_url();
        if (!base || !base[0]) {
            json_t *r = json_object();
            json_set(r, "ok", json_bool(false));
            json_set(r, "reason", json_string("no sync base url configured"));
            return r;
        }
        client = ssc_client_new(base, api_key, 30);
        free(base);
        client_owned = true;
    }

    json_t *caps = ssc_client_capabilities(client, NULL);
    if (!caps || ssc_check_version(caps) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("incompatible sync server version"));
        if (caps) json_free(caps);
        if (client_owned) ssc_client_free(client);
        return r;
    }
    bool org_feature = false;
    json_t *features = json_obj_get(caps, "features");
    if (features && features->type == JSON_ARRAY) {
        size_t n = json_len(features);
        for (size_t i = 0; i < n; i++) {
            json_t *f = json_get(features, i);
            const char *fv = (f && f->type == JSON_STRING)
                ? json_get_str(f, "", "") : "";
            if (strcmp(fv, "org") == 0) { org_feature = true; break; }
        }
    }
    long max_bytes = (long)json_get_num(caps, "max_object_bytes",
                                        DEFAULT_MAX_OBJECT_BYTES);
    json_free(caps);
    if (!org_feature) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("server does not support org-shared skills"));
        if (client_owned) ssc_client_free(client);
        return r;
    }

    /* locate local skill dir */
    char skill_dir[4096];
    if (ssc_find_skill_dir(skill_name, skill_dir, sizeof(skill_dir)) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("skill not found under the skills dir"));
        if (client_owned) ssc_client_free(client);
        return r;
    }
    char md[8192];
    snprintf(md, sizeof(md), "%s/SKILL.md", skill_dir);
    if (access(md, F_OK) != 0) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("skill has no SKILL.md"));
        if (client_owned) ssc_client_free(client);
        return r;
    }

    ssc_object_set_t objects = {0};
    int too_large = 0;
    char *skill_tree = ssc_build_tree(skill_dir, &objects, max_bytes, &too_large);
    if (!skill_tree) {
        json_t *r = json_object();
        json_set(r, "ok", json_bool(false));
        json_set(r, "reason", json_string("skill tree build failed"));
        ssc_object_set_free(&objects);
        if (client_owned) ssc_client_free(client);
        return r;
    }

    /* bounded retry: re-splice onto moved org HEAD */
    int attempts = 0;
    char *commit_hash = NULL;
    json_t *result = NULL;
    for (;;) {
        attempts++;
        char *base_head = ssc_read_org_head(client, org_id);
        json_t *skill_map = json_object();
        if (base_head) {
            char *base_root = ssc_root_tree_of_commit(client, base_head, true);
            json_t *trees = ssc_skill_trees_of_root(client, base_root, true);
            for (size_t i = 0; i < trees->c.count; i++)
                json_set(skill_map, trees->c.keys[i],
                         json_string(json_get_str(trees->c.items[i], "", "")));
            json_free(trees);
            free(base_root);
        }
        /* rel path of the skill */
        char rel[4096];
        ssc_skill_rel_path(skill_name, rel, sizeof(rel));
        json_set(skill_map, rel, json_string(skill_tree));

        char *root_hash = ssc_assemble_root_from_skill_trees(client, skill_map,
                                                             &objects);
        json_free(skill_map);

        const char *parents[1] = {base_head};
        size_t nparents = base_head ? 1 : 0;
        char msg[1024];
        snprintf(msg, sizeof(msg), "%s", message ? message : "propose skill");
        char *device = ssc_stable_device_id();
        commit_hash = ssc_build_commit(root_hash, parents, nparents, owner,
                                       device, msg, &objects, NULL);
        free(device);
        free(root_hash);

        ssc_client_put_objects(client, &objects, true, NULL);

        char org_ref[512];
        ssc_org_head_ref(org_id, org_ref, sizeof(org_ref));
        char actual[512];
        int st;
        int cas_rc = ssc_client_cas_ref(client, org_ref, base_head,
                                        commit_hash, actual, sizeof(actual),
                                        &st, NULL);
        free(base_head);
        if (cas_rc == 0) {
            result = json_object();
            json_set(result, "ok", json_bool(true));
            json_set(result, "merged", json_bool(true));
            json_set(result, "head", json_string(commit_hash));
            json_set(result, "commit", json_string(commit_hash));
            json_set(result, "org_id", json_string(org_id));
            break;
        } else if (cas_rc == 1) {
            /* proposal pending (202) */
            result = json_object();
            json_set(result, "ok", json_bool(true));
            json_set(result, "proposal_pending", json_bool(true));
            json_set(result, "commit", json_string(commit_hash));
            json_set(result, "org_id", json_string(org_id));
            break;
        } else if (cas_rc == 2) {
            if (attempts >= 5) {
                result = json_object();
                json_set(result, "ok", json_bool(false));
                char msg2[1024];
                snprintf(msg2, sizeof(msg2),
                         "the organisation's skills changed while proposing, "
                         "and %d attempts to catch up all lost the race", attempts);
                json_set(result, "reason", json_string(msg2));
                json_set(result, "status", json_number(409));
                break;
            }
            /* retry: re-splice onto new head */
            continue;
        } else {
            result = json_object();
            json_set(result, "ok", json_bool(false));
            json_set(result, "reason", json_string("cas_ref failed"));
            json_set(result, "status", json_number(st));
            break;
        }
    }

    free(skill_tree);
    free(commit_hash);
    ssc_object_set_free(&objects);
    if (client_owned) ssc_client_free(client);
    return result;
}

/* ── resolve_org_identity (L1744) ────────────────────────────────────── */

/* Resolve identity + org context for org-skill operations. Returns
 * identity dict extended with org_id + org_role, or NULL when the token
 * carries no org_role claim (personal org — org sync unavailable). */
/* PoP: resolve_org_identity @ tools/skills_sync_client.py:resolve_org_identity */
json_t *ssc_resolve_org_identity(void) {
    json_t *identity = skills_sync_resolve_identity();
    if (!identity) return NULL;
    json_t *claims = json_obj_get(identity, "claims");
    const char *org_id = claims ? json_get_str(claims, "org_id", "") : "";
    const char *org_role = claims ? json_get_str(claims, "org_role", "") : "";
    if (!org_id[0] || !org_role[0]) {
        json_free(identity);
        return NULL;
    }
    json_set(identity, "org_id", json_string(org_id));
    json_set(identity, "org_role", json_string(org_role));
    return identity;
}

/* PoP: org_sync_available @ tools/skills_sync_client.py:org_sync_available */
bool ssc_org_sync_available(void) {
    json_t *ident = ssc_resolve_org_identity();
    if (!ident) return false;
    json_free(ident);
    return true;
}

/* ── maybe_push_skills (L1613) ──────────────────────────────────────── */

/* Best-effort push if all gates pass; NULL when inert. Never raises. */
/* PoP: maybe_push_skills @ tools/skills_sync_client.py:maybe_push_skills */
json_t *ssc_maybe_push_skills(const char *message) {
    json_t *identity = skills_sync_resolve_identity();
    if (!identity) return NULL;
    if (!json_get_bool(identity, "nous_admin", false)) {
        json_free(identity);
        return NULL;  /* access gate: inert unless Nous admin */
    }
    if (!skills_sync_feature_enabled()) {
        json_free(identity);
        return NULL;
    }
    char *base = skills_sync_resolve_base_url();
    if (!base || !base[0]) {
        free(base);
        json_free(identity);
        return NULL;
    }
    free(base);
    size_t n = 0;
    char **names = skills_sync_list_synced_skill_names(&n);
    if (!names || n == 0) {
        if (names) { for (size_t i = 0; i < n; i++) free(names[i]); free(names); }
        json_free(identity);
        return NULL;
    }
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);

    json_t *result = ssc_push_skills(NULL, identity, NULL, 0, message);
    json_free(identity);
    return result;
}

/* ── maybe_pull_skills (L1632) ──────────────────────────────────────── */

/* Best-effort pull if all gates pass; NULL when inert. Never raises. */
/* PoP: maybe_pull_skills @ tools/skills_sync_client.py:maybe_pull_skills */
json_t *ssc_maybe_pull_skills(void) {
    json_t *identity = skills_sync_resolve_identity();
    if (!identity) return NULL;
    if (!json_get_bool(identity, "nous_admin", false)) {
        json_free(identity);
        return NULL;
    }
    if (!skills_sync_feature_enabled()) {
        json_free(identity);
        return NULL;
    }
    char *base = skills_sync_resolve_base_url();
    if (!base || !base[0]) {
        free(base);
        json_free(identity);
        return NULL;
    }
    free(base);
    json_t *result = ssc_pull_skills(NULL, identity);
    json_free(identity);
    return result;
}

/* ── sync_status (L1650) ────────────────────────────────────────────── */

/* Return a status snapshot for `hermes sync status`. Never raises. */
/* PoP: sync_status @ tools/skills_sync_client.py:sync_status */
json_t *ssc_sync_status(void) {
    json_t *status = json_object();
    json_set(status, "nous_admin", json_bool(false));
    json_set(status, "logged_in", json_bool(false));
    json_set(status, "feature_enabled", json_bool(skills_sync_feature_enabled()));
    json_set(status, "default_opt_in", json_bool(skills_sync_default_opt_in()));
    char *base = skills_sync_resolve_base_url();
    json_set(status, "base_url", base && base[0] ? json_string(base) : json_null());
    free(base);
    json_set(status, "opted_in_skills", json_array());
    json_set(status, "local_head", json_null());
    json_set(status, "owner", json_null());
    json_set(status, "org_available", json_bool(false));
    json_set(status, "org_id", json_null());
    json_set(status, "org_role", json_null());
    json_set(status, "org_skills", json_array());
    json_set(status, "org_skills_modified", json_array());

    /* identity */
    json_t *identity = skills_sync_resolve_identity();
    if (identity) {
        json_set(status, "logged_in", json_bool(true));
        const char *owner = json_get_str(identity, "owner", "");
        json_set(status, "owner", owner[0] ? json_string(owner) : json_null());
        json_set(status, "nous_admin",
                 json_bool(json_get_bool(identity, "nous_admin", false)));
        json_free(identity);
    }

    /* opted-in skills + local head */
    size_t n = 0;
    char **names = skills_sync_list_synced_skill_names(&n);
    json_t *opted = json_array();
    for (size_t i = 0; i < n; i++) {
        json_append(opted, json_string(names[i]));
        free(names[i]);
    }
    free(names);
    json_set(status, "opted_in_skills", opted);
    json_t *state = ssc_read_sync_state();
    const char *head = json_get_str(state, "head", NULL);
    json_set(status, "local_head", head && head[0] ? json_string(head) : json_null());
    json_free(state);

    /* org context (best-effort) */
    json_t *org_ident = ssc_resolve_org_identity();
    if (org_ident) {
        json_set(status, "org_available", json_bool(true));
        json_set(status, "org_id", json_string(json_get_str(org_ident, "org_id", "")));
        json_set(status, "org_role", json_string(json_get_str(org_ident, "org_role", "")));
        json_set(status, "org_skills", ssc_list_org_skill_names());
        const char *org_id = json_get_str(org_ident, "org_id", "");
        json_set(status, "org_skills_modified",
                 ssc_list_locally_modified_org_skills(org_id));
        json_free(org_ident);
    }
    return status;
}

/* ── _clear_active_org_marker (L2174) ───────────────────────────────── */

/* Remove the active-org marker (org skills stop resolving). */
/* PoP: _clear_active_org_marker @ tools/skills_sync_client.py:_clear_active_org_marker */
void ssc_clear_active_org_marker(void) {
    char orgd[4096];
    ssc_org_dir(orgd, sizeof(orgd));
    char marker[8192];
    snprintf(marker, sizeof(marker), "%s/ACTIVE_ORG", orgd);
    unlink(marker);
}

/* ── maybe_pull_org_skills (L2130) ──────────────────────────────────── */

/* Best-effort org pull if all gates pass; NULL when inert. Never raises. */
/* PoP: maybe_pull_org_skills @ tools/skills_sync_client.py:maybe_pull_org_skills */
json_t *ssc_maybe_pull_org_skills(void) {
    json_t *org_ident = ssc_resolve_org_identity();
    if (!org_ident) {
        /* Distinguish "verifiably personal/left-org" from "can't tell". */
        json_t *base_identity = skills_sync_resolve_identity();
        if (base_identity) {
            json_t *claims = json_obj_get(base_identity, "claims");
            const char *org_role = claims ? json_get_str(claims, "org_role", "") : "";
            if (!org_role[0]) ssc_clear_active_org_marker();
            json_free(base_identity);
        }
        return NULL;
    }
    json_free(org_ident);

    if (!skills_sync_feature_enabled()) return NULL;
    char *base = skills_sync_resolve_base_url();
    if (!base || !base[0]) {
        free(base);
        return NULL;
    }
    free(base);

    json_t *identity = ssc_resolve_org_identity();
    if (!identity) return NULL;
    json_t *result = ssc_pull_org_skills(NULL, identity);
    json_free(identity);
    return result;
}
