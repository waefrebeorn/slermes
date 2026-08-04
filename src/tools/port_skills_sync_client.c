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
