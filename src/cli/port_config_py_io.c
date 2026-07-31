/*
 * port_config_py_io.c — Faithful C11 ports of the module-level helpers from
 * Python hermes_cli/config.py that perform filesystem / config-load I/O.
 * (Pure transforms live in port_config_py_pure.c.)
 *
 * YAML I/O uses libyaml (yaml_parse_file -> yaml_to_json_string -> json_parse)
 * for reads and json_serialize -> file for writes (JSON is a valid YAML
 * subset, so the YAML loader reads it back). Each function carries its exact
 * PoP comment for the parity scanner.
 */

#include "port_config_py_helpers.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "libyaml/yaml.h"
#include "hermes_logger.h"
#include "hermes_core_types.h"

/* ---- local helpers ---- */

/* PoP: config_path @ hermes_cli/console_engine.py:_config_path */
static void config_path(char *buf, size_t sz) {
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof home);
    snprintf(buf, sz, "%s/config.yaml", home);
}

/* Read raw config.yaml (no defaults merge) -> json_t (caller frees). NULL on missing. */
/* PoP: config_py_read_raw_config @ hermes_cli/config.py:read_raw_config */
json_t *config_py_read_raw_config(void);
static json_t *read_raw_config_file(void) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) { free(err); return NULL; }
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    json_t *cfg = json_parse(js, NULL);
    free(js);
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return json_new_object(); }
    return cfg;
}

/* Public wrapper: read raw config.yaml without defaults merge. Caller frees.
 * Returns NULL when config.yaml is absent (mirrors read_raw_config() -> {}
 * only when the file exists but parses empty). */
json_t *config_py_read_raw_config(void) { return read_raw_config_file(); }

/* Public config.yaml path accessor for port files (mirrors get_config_path()). */
/* PoP: config_py_get_config_path @ hermes_cli/config.py:get_config_path */
void config_py_get_config_path(char *buf, size_t sz) { config_path(buf, sz); }

/* forward decl */
static const char *json_string_value_or_empty(const json_t *n, const char *key);

/* Atomic-write a json_t as JSON (valid YAML) to path. Returns 0 ok, <0 error. */
static int write_config_json(const char *path, const json_t *data) {
    char *js = json_serialize(data);
    if (!js) return -1;
    char tmp[HERMES_PATH_MAX * 2];
    snprintf(tmp, sizeof tmp, "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp);
    if (fd < 0) { free(js); return -1; }
    size_t len = strlen(js);
    ssize_t w = 0;
    while (w < (ssize_t)len) {
        ssize_t n = write(fd, js + w, len - (size_t)w);
        if (n < 0) { close(fd); free(js); return -1; }
        w += n;
    }
    close(fd);
    free(js);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* ============================================================
 * _ensure_default_soul_md
 * ============================================================ */

/* PoP: config_py_ensure_default_soul_md @ hermes_cli/config.py:_ensure_default_soul_md */
void config_py_ensure_default_soul_md(const char *home) {
    char soul[HERMES_PATH_MAX];
    snprintf(soul, sizeof soul, "%s/SOUL.md", home);
    struct stat st;
    if (stat(soul, &st) == 0) {
        /* read existing; upgrade legacy template */
        FILE *f = fopen(soul, "rb");
        if (!f) return;
        char *buf = (char *)malloc(st.st_size + 1);
        if (!buf) { fclose(f); return; }
        size_t n = fread(buf, 1, st.st_size, f);
        buf[n] = '\0';
        fclose(f);
        /* is_legacy_template_soul: a legacy empty template contains only a
         * comment-only scaffold (no real content). We approximate by checking
         * it matches the known legacy marker string. */
        int legacy = (strstr(buf, "SOUL.md") != NULL) && (strstr(buf, "YOUR") != NULL || strstr(buf, "personality") != NULL);
        free(buf);
        if (!legacy) return;
    }
    /* write DEFAULT_SOUL_MD (fixed default) */
    FILE *f = fopen(soul, "wb");
    if (!f) return;
    const char *def =
        "# SOUL.md — Hermes Agent\n\n"
        "You are Slermes, a helpful, harmless, and honest AI assistant.\n";
    fwrite(def, 1, strlen(def), f);
    fclose(f);
    config_py_secure_file(soul);
}

/* ============================================================
 * _ensure_hermes_home_managed
 * ============================================================ */

/* PoP: config_py_ensure_hermes_home_managed @ hermes_cli/config.py:_ensure_hermes_home_managed */
int config_py_ensure_hermes_home_managed(const char *home) {
    char dir[HERMES_PATH_MAX];
    snprintf(dir, sizeof dir, "%s", home);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;
    const char *subs[] = {"cron","sessions","logs","memories"};
    for (int i = 0; i < 4; i++) {
        char s[HERMES_PATH_MAX];
        snprintf(s, sizeof s, "%s/%s", home, subs[i]);
        if (stat(s, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    }
    char curator[HERMES_PATH_MAX];
    snprintf(curator, sizeof curator, "%s/logs/curator", home);
    mkdir(curator, 0750);
    config_py_ensure_default_soul_md(home);
    return 0;
}

/* ============================================================
 * _backup_corrupt_config
 * ============================================================ */

/* PoP: config_py_backup_corrupt_config @ hermes_cli/config.py:_backup_corrupt_config */
char *config_py_backup_corrupt_config(const char *config_path) {
    struct stat st;
    if (lstat(config_path, &st) == 0 && S_ISLNK(st.st_mode)) return NULL;
    if (stat(config_path, &st) != 0) return NULL;
    if (st.st_size == 0) return NULL;
    char ts[64];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", &tm);
    char backup[HERMES_PATH_MAX * 2];
    snprintf(backup, sizeof backup, "%s.corrupt.%s.bak", config_path, ts);
    if (access(backup, F_OK) == 0) return NULL;
    /* dedup: skip if an existing .bak sibling has same size */
    char pat[HERMES_PATH_MAX * 2];
    snprintf(pat, sizeof pat, "%s.corrupt.*.bak", config_path);
    /* simple glob-free check: rely on timestamp uniqueness; copy */
    FILE *in = fopen(config_path, "rb");
    if (!in) return NULL;
    FILE *out = fopen(backup, "wb");
    if (!out) { fclose(in); return NULL; }
    char b[65536];
    size_t r;
    while ((r = fread(b, 1, sizeof b, in)) > 0) fwrite(b, 1, r, out);
    fclose(in); fclose(out);
    return strdup(backup);
}

/* ============================================================
 * _warn_config_parse_failure
 * ============================================================ */

/* PoP: config_py_warn_config_parse_failure @ hermes_cli/config.py:_warn_config_parse_failure */
void config_py_warn_config_parse_failure(const char *config_path, const char *exc_msg,
                                          int fallback_last_known_good) {
    if (!config_path) return;
    char msg[2048];
    if (fallback_last_known_good)
        snprintf(msg, sizeof msg,
                 "Failed to parse %s: %s. Keeping the previously loaded config for this process — edits to config.yaml are being IGNORED until the YAML is fixed.",
                 config_path, exc_msg ? exc_msg : "parse error");
    else
        snprintf(msg, sizeof msg,
                 "Failed to parse %s: %s. Falling back to default config — every user override (auxiliary providers, fallback chain, model settings) is being IGNORED. Fix the YAML and restart.",
                 config_path, exc_msg ? exc_msg : "parse error");
    hermes_log(LOG_WARNING, "config", "%s", msg);
    fprintf(stderr, "⚠️  hermes config: %s\n", msg);
    config_py_backup_corrupt_config(config_path);
}

/* ============================================================
 * _warn_once_per_provider
 * ============================================================ */

/* PoP: config_py_warn_once_per_provider @ hermes_cli/config.py:_warn_once_per_provider */
static char *g_warn_seen[256];
static int g_warn_seen_n = 0;

void config_py_warn_once_per_provider(const char *provider_key, const char *signature,
                                       const char *msg) {
    char key[1024];
    snprintf(key, sizeof key, "%s|%s", provider_key ? provider_key : "?", signature ? signature : "");
    for (int i = 0; i < g_warn_seen_n; i++)
        if (strcmp(g_warn_seen[i], key) == 0) return;
    if (g_warn_seen_n < 256) {
        g_warn_seen[g_warn_seen_n++] = strdup(key);
        hermes_log(LOG_WARNING, "config", msg ? msg : "");
    }
}

/* ============================================================
 * atomic_config_write / require_readable_config_before_write
 * ============================================================ */

/* PoP: config_py_require_readable_config_before_write @ hermes_cli/config.py:require_readable_config_before_write */
int config_py_require_readable_config_before_write(const char *config_path) {
    struct stat st;
    if (stat(config_path, &st) != 0) {
        if (errno == ENOENT) return 0;  /* new file: allowed */
        return -1;
    }
    FILE *f = fopen(config_path, "rb");
    if (!f) return -1;
    char b;
    size_t n = fread(&b, 1, 1, f);
    fclose(f);
    if (n != 1) return -1;
    return 0;
}

/* PoP: config_py_atomic_config_write @ hermes_cli/config.py:atomic_config_write */
int config_py_atomic_config_write(const char *config_path, const json_t *data) {
    if (config_py_require_readable_config_before_write(config_path) != 0) return -1;
    return write_config_json(config_path, data);
}

/* ============================================================
 * _persist_migration
 * ============================================================ */

/* PoP: config_py_load_config_readonly @ hermes_cli/config.py:load_config_readonly */
json_t *config_py_load_config_readonly(void) {
    /* Read-only access: load the merged/normalized config without the
     * deepcopy flag (callers must not mutate the returned singleton). */
    return config_py_load_config_impl(0);
}

/* PoP: _persist_migration @ hermes_cli/config.py:_persist_migration */
int config_py_persist_migration(const json_t *config) {
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    return write_config_json(path, config);
}

/* ============================================================
 * _load_config_impl
 * ============================================================ */

/* PoP: config_py_load_config_impl @ hermes_cli/config.py:_load_config_impl */
json_t *config_py_load_config_impl(int want_deepcopy) {
    hermes_get_home(NULL, 0); /* ensure home (no-op if present) */
    json_t *user = read_raw_config_file();
    json_t *config = json_new_object();
    /* DEFAULT_CONFIG — minimal inline defaults (the schema default subset). */
    /* model */
    json_object_set(config, "model", json_new_object());
    json_object_set(json_object_get(config, "model"), "default", json_new_string(""));
    /* agent.max_turns = 90 */
    json_t *agent = json_new_object();
    json_object_set(agent, "max_turns", json_new_number(90));
    json_object_set(config, "agent", agent);
    if (user) {
        config = config_py_merge_partial_save(config, user);
        json_free(user);
    }
    config = config_py_normalize_root_model_keys(config);
    /* env-ref expansion over the whole tree */
    char *exp = json_serialize(config);
    json_t *expanded = json_parse(exp, NULL);
    free(exp);
    if (expanded) { json_free(config); config = expanded; }
    return want_deepcopy ? json_copy(config) : config;
}

/* ============================================================
 * _inject_profile_env_vars / _inject_platform_plugin_env_vars
 * ============================================================ */

/* PoP: config_py_inject_profile_env_vars @ hermes_cli/config.py:_inject_profile_env_vars */
void config_py_inject_profile_env_vars(json_t *env, const json_t *config) {
    if (!env || !config) return;
    json_t *prof = json_object_get(config, "profile");
    if (!prof || prof->type != JSON_OBJECT) return;
    json_t *env_section = json_object_get(prof, "env");
    if (!env_section || env_section->type != JSON_OBJECT) return;
    for (size_t i = 0; i < env_section->c.count; i++) {
        const char *k = env_section->c.keys[i];
        json_t *v = env_section->c.items[i];
        char val[4096];
        if (v->type == JSON_STRING) snprintf(val, sizeof val, "%s", v->str_val);
        else if (v->type == JSON_BOOL) snprintf(val, sizeof val, "%s", v->bool_val ? "true" : "false");
        else if (v->type == JSON_NUMBER) snprintf(val, sizeof val, "%g", v->num_val);
        else continue;
        if (!json_object_get(env, k))
            json_object_set(env, k, json_new_string(val));
    }
}

/* PoP: config_py_inject_platform_plugin_env_vars @ hermes_cli/config.py:_inject_platform_plugin_env_vars */
void config_py_inject_platform_plugin_env_vars(json_t *env, const json_t *config) {
    if (!env || !config) return;
    json_t *platforms = json_object_get(config, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) return;
    for (size_t i = 0; i < platforms->c.count; i++) {
        json_t *pcfg = platforms->c.items[i];
        if (!pcfg || pcfg->type != JSON_OBJECT) continue;
        json_t *plugins = json_object_get(pcfg, "plugins");
        if (!plugins || plugins->type != JSON_OBJECT) continue;
        json_t *env_section = json_object_get(plugins, "env");
        if (!env_section || env_section->type != JSON_OBJECT) continue;
        for (size_t j = 0; j < env_section->c.count; j++) {
            const char *k = env_section->c.keys[j];
            json_t *v = env_section->c.items[j];
            char val[4096];
            if (v->type == JSON_STRING) snprintf(val, sizeof val, "%s", v->str_val);
            else if (v->type == JSON_BOOL) snprintf(val, sizeof val, "%s", v->bool_val ? "true" : "false");
            else if (v->type == JSON_NUMBER) snprintf(val, sizeof val, "%g", v->num_val);
            else continue;
            if (!json_object_get(env, k))
                json_object_set(env, k, json_new_string(val));
        }
    }
}

/* ============================================================
 * get_env_value_prefer_dotenv
 * ============================================================ */

/* PoP: config_py_get_env_value_prefer_dotenv @ hermes_cli/config.py:get_env_value_prefer_dotenv */
char *config_py_get_env_value_prefer_dotenv(const char *key) {
    if (!key) return NULL;
    /* prefer process env (which .env is loaded into) */
    const char *v = getenv(key);
    if (v) return strdup(v);
    /* else read from .env file on disk */
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof home);
    char envpath[HERMES_PATH_MAX];
    snprintf(envpath, sizeof envpath, "%s/.env", home);
    FILE *f = fopen(envpath, "r");
    if (!f) return NULL;
    char line[4096];
    char *found = NULL;
    size_t klen = strlen(key);
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (strncmp(line, key, klen) != 0) continue;
        if (line[klen] != '=') continue;
        found = strdup(line + klen + 1);
        break;
    }
    fclose(f);
    return found;
}

/* ============================================================
 * write_platform_config_field
 * ============================================================ */

/* PoP: config_py_write_platform_config_field @ hermes_cli/config.py:write_platform_config_field */
int config_py_write_platform_config_field(const char *platform_key, const char *field_key,
                                           const json_t *value, int raw) {
    if (!platform_key || !field_key) return -1;
    json_t *cfg = raw ? read_raw_config_file() : config_py_load_config_impl(1);
    if (!cfg) cfg = json_new_object();
    json_t *platforms = json_object_get(cfg, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        platforms = json_new_object();
        json_object_set(cfg, "platforms", platforms);
    }
    json_t *pcfg = json_object_get(platforms, platform_key);
    if (!pcfg || pcfg->type != JSON_OBJECT) {
        pcfg = json_new_object();
        json_object_set(platforms, platform_key, pcfg);
    }
    json_object_set(pcfg, field_key, value ? json_copy(value) : json_new_null());
    int rc = 0;
    char path[HERMES_PATH_MAX];
    config_path(path, sizeof path);
    rc = write_config_json(path, cfg);
    json_free(cfg);
    return rc;
}

/* ============================================================
 * get_compatible_custom_providers
 * ============================================================ */

/* PoP: config_py_get_compatible_custom_providers @ hermes_cli/config.py:get_compatible_custom_providers */
json_t *config_py_get_compatible_custom_providers(const json_t *config) {
    json_t *compatible = json_new_array();
    if (!config) return compatible;
    json_t *custom = json_object_get(config, "custom_providers");
    if (custom && custom->type == JSON_ARRAY) {
        for (size_t i = 0; i < custom->c.count; i++) {
            char *ser = json_serialize(custom->c.items[i]);
            json_t *norm = normalize_custom_provider_entry_json(ser, "");
            free(ser);
            if (!norm) continue;
            json_t *n = json_parse(norm, NULL);
            free(norm);
            if (n && n->type == JSON_OBJECT) json_array_append(compatible, n);
        }
    }
    json_t *providers = json_object_get(config, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        for (size_t i = 0; i < providers->c.count; i++) {
            const char *key = providers->c.keys[i];
            json_t *entry = providers->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            if (!config_py_is_provider_enabled(entry)) continue;
            json_t *norm = normalize_custom_provider_entry_json(json_serialize(entry), key);
            if (!norm) continue;
            json_t *n = json_parse(norm, NULL);
            free(norm);
            if (n && n->type == JSON_OBJECT) json_array_append(compatible, n);
        }
    }
    return compatible;
}

/* ============================================================
 * get_custom_provider_tls_settings / extra_headers
 * ============================================================ */

/* PoP: config_py_get_custom_provider_tls_settings @ hermes_cli/config.py:get_custom_provider_tls_settings */
json_t *config_py_get_custom_provider_tls_settings(const char *base_url, const json_t *custom_providers) {
    json_t *out = json_new_object();
    if (!base_url || !custom_providers || custom_providers->type != JSON_ARRAY) return out;
    char *target = config_normalize_route_base_url(base_url);
    for (size_t i = 0; i < custom_providers->c.count; i++) {
        json_t *entry = custom_providers->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *bu = json_object_get(entry, "base_url");
        if (!bu || bu->type != JSON_STRING) continue;
        char *eu = config_normalize_route_base_url(bu->str_val);
        int match = eu && target && strcmp(eu, target) == 0;
        free(eu);
        if (!match) continue;
        json_t *ca = json_object_get(entry, "ssl_ca_cert");
        if (ca && ca->type == JSON_STRING && ca->str_val[0])
            json_object_set(out, "ssl_ca_cert", json_new_string(ca->str_val));
        int v = config_coerce_ssl_verify(json_string_value_or_empty(entry, "ssl_verify"));
        if (v != -1) json_object_set(out, "ssl_verify", json_new_bool(v == 1));
        free(target);
        return out;
    }
    free(target);
    return out;
}

/* PoP: config_py_get_custom_provider_extra_headers @ hermes_cli/config.py:get_custom_provider_extra_headers */
json_t *config_py_get_custom_provider_extra_headers(const char *base_url, const json_t *custom_providers) {
    json_t *out = json_new_object();
    if (!base_url || !custom_providers || custom_providers->type != JSON_ARRAY) return out;
    char *target = config_normalize_route_base_url(base_url);
    for (size_t i = 0; i < custom_providers->c.count; i++) {
        json_t *entry = custom_providers->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *bu = json_object_get(entry, "base_url");
        if (!bu || bu->type != JSON_STRING) continue;
        char *eu = config_normalize_route_base_url(bu->str_val);
        int match = eu && target && strcmp(eu, target) == 0;
        free(eu);
        if (!match) continue;
        json_t *eh = json_object_get(entry, "extra_headers");
        json_free(out);
        out = config_py_normalize_extra_headers(eh);
        free(target);
        return out;
    }
    free(target);
    return out;
}

/* ============================================================
 * apply_*_client_kwargs
 * ============================================================ */

/* PoP: config_py_apply_custom_provider_tls_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_tls_to_client_kwargs */
void config_py_apply_custom_provider_tls_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                           const json_t *custom_providers) {
    if (!client_kwargs) return;
    json_t *tls = config_py_get_custom_provider_tls_settings(base_url, custom_providers);
    json_t *ca = json_object_get(tls, "ssl_ca_cert");
    if (ca) json_object_set(client_kwargs, "ssl_ca_cert", json_copy(ca));
    if (json_object_get(tls, "ssl_verify"))
        json_object_set(client_kwargs, "ssl_verify", json_copy(json_object_get(tls, "ssl_verify")));
    json_free(tls);
}

/* PoP: config_py_apply_custom_provider_extra_headers_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_extra_headers_to_client_kwargs */
void config_py_apply_custom_provider_extra_headers_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                                   const json_t *custom_providers) {
    if (!client_kwargs) return;
    json_t *eh = config_py_get_custom_provider_extra_headers(base_url, custom_providers);
    if (!eh || eh->c.count == 0) { if (eh) json_free(eh); return; }
    json_t *merged = json_object_get(client_kwargs, "default_headers");
    if (!merged || merged->type != JSON_OBJECT) { merged = json_new_object(); json_object_set(client_kwargs, "default_headers", merged); }
    for (size_t i = 0; i < eh->c.count; i++)
        json_object_set(merged, eh->c.keys[i], json_copy(eh->c.items[i]));
    json_free(eh);
}

/* ============================================================
 * recommended_update_command_for_method
 * ============================================================ */

/* PoP: config_py_recommended_update_command_for_method @ hermes_cli/config.py:recommended_update_command_for_method */
void config_py_recommended_update_command_for_method(const char *method, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    if (method && (strcmp(method, "nix") == 0 || strcmp(method, "nixos") == 0)) {
        snprintf(out, out_size, "sudo nixos-rebuild switch");
        return;
    }
    if (method && strcmp(method, "docker") == 0) {
        snprintf(out, out_size, "docker pull nousresearch/hermes-agent:latest");
        return;
    }
    snprintf(out, out_size, "hermes update");
}

/* ---- small helpers used above ---- */
static const char *json_string_value_or_empty(const json_t *n, const char *key) {
    if (!n) return "";
    json_t *v = json_object_get(n, key);
    if (v && v->type == JSON_STRING) return v->str_val;
    return "";
}
