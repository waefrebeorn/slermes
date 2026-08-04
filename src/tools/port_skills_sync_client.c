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
static char *canonical_json_bytes(json_t *obj, size_t *out_len) {
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

    char *bytes = canonical_json_bytes(manifest, out_len);
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

/* ── sync feature knobs (L391-435) ────────────────────────────────────── */

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