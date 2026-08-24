/*
 * plugins_cmd.c — canonical C11 port of the command handlers in
 * hermes_cli/plugins_cmd.py and hermes_cli/plugin_packs.py.
 *
 * Real argument handling, validation, config read/modify/write, plugin
 * discovery (via plugins_core scanning), git install/remove fan-out and
 * community-index resolution (via plugin_index_pure). Interactive rich /
 * curses surfaces degrade to their plain-text equivalents; the curses
 * composite UI boundary is the non-TTY path.
 */

#define _POSIX_C_SOURCE 200809L
#include "plugins_cmd.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "plugin_index_pure.h"
#include "plugins_core.h"

/* sibling ports (defined in plugins_cmd_wrappers.c / plugin_packs_pure.c) */
extern char *pcmd_resolve_git_url(const char *identifier, char **out_subdir);
extern const char *pcmd_resolve_git_executable(void);
extern int plugin_pack_is_exact_sha(const char *ref);

/* relay_plugin_cutover.py constants */
static const char *const LEGACY_RELAY_PLUGIN_KEYS[] = {
    "nemo_relay", "observability/nemo_relay", NULL,
};
#define RELAY_PLUGINS_CONFIG_ENV "HERMES_NEMO_RELAY_PLUGINS_TOML"
#define INSTALL_METADATA_FILE ".install-metadata.json"
#define DEFAULT_INDEX_URL \
    "https://raw.githubusercontent.com/NousResearch/hermes-plugin-index/main/index.json"

/* ------------------------------------------------------------------ */
/* small utilities                                                     */
/* ------------------------------------------------------------------ */

static void p_home(char *buf, size_t sz) { hermes_get_home(buf, sz); }

static json_t *p_cfg_load(void) { return config_py_load_config_readonly(); }

static int p_is_dir(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int p_exists(const char *path) {
    struct stat st;
    return path && (stat(path, &st) == 0 || lstat(path, &st) == 0);
}

static int p_child_exists(const char *dir, const char *name) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return p_exists(path);
}

static char *p_read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* Python shutil.rmtree — recursive delete. Returns 0 on success. */
static int p_rm_rf(const char *path) {
    DIR *d = opendir(path);
    if (!d) return remove(path);
    struct dirent *e;
    int failed = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        char sub[4096];
        snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(sub, &st) != 0) continue;
        failed |= S_ISDIR(st.st_mode) ? p_rm_rf(sub) : (remove(sub) != 0);
    }
    closedir(d);
    failed |= rmdir(path) != 0;
    return failed ? -1 : 0;
}

/* ---- string-set helpers (Python set semantics on config name lists) ---- */

typedef struct {
    char **v;
    size_t n, cap;
} strset_t;

static int strset_contains(const strset_t *s, const char *val);

static void strset_free(strset_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

static void strset_push(strset_t *s, const char *val) {
    if (!val || !*val) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = realloc(s->v, s->cap * sizeof(char *));
    }
    s->v[s->n++] = strdup(val);
}

static void strset_add(strset_t *s, const char *val) {
    if (!strset_contains(s, val)) strset_push(s, val);
}

static int strset_contains(const strset_t *s, const char *val) {
    if (!val) return 0;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], val) == 0) return 1;
    return 0;
}

static void strset_discard(strset_t *s, const char *val) {
    if (!val) return;
    for (size_t i = 0; i < s->n; i++) {
        if (strcmp(s->v[i], val) == 0) {
            free(s->v[i]);
            memmove(&s->v[i], &s->v[i + 1], (s->n - i - 1) * sizeof(char *));
            s->n--;
            return;
        }
    }
}

static void strset_from_json(strset_t *out, const json_t *arr) {
    memset(out, 0, sizeof *out);
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len((json_t *)arr); i++) {
        const json_t *v = json_get((json_t *)arr, i);
        if (v && v->type == JSON_STRING) strset_push(out, v->str_val);
    }
}

static json_t *strset_to_json(const strset_t *s) {
    json_t *arr = json_array();
    for (size_t i = 0; i < s->n; i++) json_append(arr, json_string(s->v[i]));
    return arr;
}

static int p_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Persist a name list to config.yaml under plugins.<key>, sorted. */
static int p_save_name_set(const char *key, const strset_t *set) {
    const char **names = malloc((set->n ? set->n : 1) * sizeof(char *));
    for (size_t i = 0; i < set->n; i++) names[i] = set->v[i];
    qsort(names, set->n, sizeof(char *), p_cmp_str);
    json_t *sorted = json_array();
    for (size_t i = 0; i < set->n; i++)
        if (i == 0 || strcmp(names[i], names[i - 1]) != 0)
            json_append(sorted, json_string(names[i]));
    free(names);
    char dotted[64];
    snprintf(dotted, sizeof dotted, "plugins.%s", key);
    int rc = config_py_save_value(dotted, sorted);
    json_free(sorted);
    return rc;
}

static void p_get_name_set(const char *key, strset_t *out) {
    json_t *cfg = p_cfg_load();
    const json_t *plug = cfg ? json_obj_get(cfg, "plugins") : NULL;
    const json_t *arr =
        (plug && plug->type == JSON_OBJECT) ? json_obj_get(plug, key) : NULL;
    strset_from_json(out, arr);
    if (cfg) json_free(cfg);
}

/* Read a plugin manifest (plugin.yaml/yml/json) as JSON. Caller frees. */
static json_t *p_manifest_read(const char *dir) {
    static const char *const YAML_NAMES[] = {"plugin.yaml", "plugin.yml", NULL};
    char path[4096];
    for (int i = 0; YAML_NAMES[i]; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, YAML_NAMES[i]);
        if (!p_exists(path)) continue;
        char *text = p_read_text(path);
        if (!text) return NULL;
        json_t *m = json_parse_yaml(text);
        free(text);
        if (m && m->type != JSON_OBJECT) { json_free(m); return NULL; }
        return m;
    }
    snprintf(path, sizeof path, "%s/plugin.json", dir);
    if (p_exists(path)) {
        char *text = p_read_text(path);
        if (!text) return NULL;
        json_t *m = json_parse(text, NULL);
        free(text);
        if (m && m->type != JSON_OBJECT) { json_free(m); return NULL; }
        return m;
    }
    return NULL;
}

static const char *p_json_str(const json_t *obj, const char *key,
                              const char *dflt) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val[0]) ? v->str_val : dflt;
}

static int p_is_legacy_relay_key(const char *name) {
    if (!name) return 0;
    for (int i = 0; LEGACY_RELAY_PLUGIN_KEYS[i]; i++)
        if (strcmp(name, LEGACY_RELAY_PLUGIN_KEYS[i]) == 0) return 1;
    return 0;
}

/* Resolve a user-supplied identifier against every discoverable plugin.
 * Mirrors _resolve_plugin_key_and_source: exact key/name first, then a
 * unique bare-leaf match. Returns 1 + fills out_key/out_src, else 0. */
static int p_resolve_key_and_source(const char *name, char *out_key,
                                    size_t key_sz, char *out_src,
                                    size_t src_sz) {
    out_key[0] = out_src[0] = '\0';
    if (!name || !*name) return 0;
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    size_t n = plugins_scan_count(r);
    int found = 0;

    for (size_t i = 0; i < n && !found; i++) {
        const char *key = plugins_scan_key(r, i);
        const char *nm = plugins_scan_name(r, i);
        if ((key && strcmp(key, name) == 0) || (nm && strcmp(nm, name) == 0)) {
            snprintf(out_key, key_sz, "%s", key ? key : name);
            snprintf(out_src, src_sz, "%s",
                     plugins_scan_source(r, i) ? plugins_scan_source(r, i) : "");
            found = 1;
        }
    }
    if (!found) {
        int matches = 0;
        const char *mk = NULL, *ms = NULL;
        for (size_t i = 0; i < n; i++) {
            const char *key = plugins_scan_key(r, i);
            if (!key) continue;
            const char *slash = strrchr(key, '/');
            const char *leaf = slash ? slash + 1 : key;
            if (strcmp(leaf, name) == 0) {
                matches++;
                mk = key;
                ms = plugins_scan_source(r, i);
            }
        }
        if (matches == 1) {
            snprintf(out_key, key_sz, "%s", mk);
            snprintf(out_src, src_sz, "%s", ms ? ms : "");
            found = 1;
        }
    }
    plugins_scan_free(r);
    return found;
}

static void p_status_of(const char *name, const char *key,
                        const strset_t *enabled, const strset_t *disabled,
                        char *out, size_t sz) {
    if (strset_contains(disabled, name) || (key && strset_contains(disabled, key)))
        snprintf(out, sz, "disabled");
    else if (strset_contains(enabled, name) || (key && strset_contains(enabled, key)))
        snprintf(out, sz, "enabled");
    else
        snprintf(out, sz, "not enabled");
}

static void p_prompt_line(const char *prompt, char *out, size_t sz) {
    fputs(prompt, stdout);
    fflush(stdout);
    out[0] = '\0';
    char buf[256];
    if (!fgets(buf, sizeof buf, stdin)) return;
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
    while (n && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) buf[--n] = '\0';
    size_t b = 0;
    while (b < n && (buf[b] == ' ' || buf[b] == '\t')) b++;
    size_t k = 0;
    for (size_t i = b; i < n && k + 1 < sz; i++)
        out[k++] = (char)tolower((unsigned char)buf[i]);
    out[k] = '\0';
}

static int p_answer_yes(const char *s) {
    return strcmp(s, "y") == 0 || strcmp(s, "yes") == 0;
}

/* ------------------------------------------------------------------ */
/* community index loading (cache -> seed -> optional refresh)         */
/* ------------------------------------------------------------------ */

static char *p_fetch_url_curl(const char *url) {
    const char *curl = "/usr/bin/curl";
    struct stat st;
    if (stat(curl, &st) != 0) curl = "curl";
    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "%s -fsSL --max-time 10 '%s' 2>/dev/null", curl, url);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    size_t cap = 8192, len = 0;
    char *out = malloc(cap);
    if (!out) { pclose(fp); return NULL; }
    size_t got;
    while ((got = fread(out + len, 1, cap - len - 1, fp)) > 0) {
        len += got;
        out[len] = '\0';
        if (cap - len < 2) {
            if (cap > 5u * 1024 * 1024) break; /* _MAX_INDEX_BYTES guard */
            cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
    }
    int rc = pclose(fp);
    if (rc != 0 || len == 0) { free(out); return NULL; }
    return out;
}

/* load_index(): remote (when refresh) -> cached copy -> bundled seed.
 * Returns malloc'd index JSON text or NULL. */
static char *p_load_index_json(int refresh) {
    char home[1024], cache_path[1200], seed_path[1200];
    p_home(home, sizeof home);
    snprintf(cache_path, sizeof cache_path, "%s/cache/plugin_index.json", home);
    const char *root = getenv("HERMES_PROJECT_ROOT");
    snprintf(seed_path, sizeof seed_path, "%s/hermes_cli/data/plugin_index.json",
             root && *root ? root : ".");

    if (refresh) {
        char *fresh = p_fetch_url_curl(DEFAULT_INDEX_URL);
        if (fresh) {
            FILE *f = fopen(cache_path, "wb");
            if (f) { fwrite(fresh, 1, strlen(fresh), f); fclose(f); }
            return fresh;
        }
    }
    if (p_exists(cache_path)) {
        char *cached = p_read_text(cache_path);
        if (cached) return cached;
    }
    if (p_exists(seed_path)) return p_read_text(seed_path);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* install / remove core                                               */
/* ------------------------------------------------------------------ */

/* Clone `git_url` into <home>/plugins/<flat_name>. Returns 0 on success. */
static int p_git_clone_into(const char *git_url, const char *flat_name,
                            char *target_out, size_t tsz) {
    char home[1024];
    p_home(home, sizeof home);
    snprintf(target_out, tsz, "%s/plugins/%s", home, flat_name);
    const char *git = pcmd_resolve_git_executable();
    char cmd[5120];
    snprintf(cmd, sizeof cmd, "%s clone --depth 1 %s %s 2>/dev/null",
             git, git_url, target_out);
    return system(cmd) != 0 ? -1 : 0;
}

static void p_flat_install_dir(const char *ident, char *out, size_t sz) {
    /* Python installs at plugins/<repo-name>; nested identifiers flatten. */
    const char *base = strrchr(ident, '/');
    base = base ? base + 1 : ident;
    size_t j = 0;
    for (size_t i = 0; base[i] && j + 1 < sz; i++) {
        char c = base[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_')
            out[j++] = (char)tolower((unsigned char)c);
    }
    out[j] = '\0';
    if (!j) snprintf(out, sz, "plugin");
}

/* Install metadata sidecar: <home>/plugins/.install-metadata.json */
/* PoP: _read_install_metadata @ hermes_cli/plugins_cmd.py:_read_install_metadata */
static json_t *p_read_install_metadata(void) {
    char home[1024], path[2048];
    p_home(home, sizeof home);
    snprintf(path, sizeof path, "%s/plugins/%s", home, INSTALL_METADATA_FILE);
    json_t *meta = NULL;
    if (p_exists(path)) {
        char *text = p_read_text(path);
        meta = text ? json_parse(text, NULL) : NULL;
        free(text);
        if (meta && meta->type != JSON_OBJECT) { json_free(meta); meta = NULL; }
    }
    return meta ? meta : json_object();
}

/* PoP: _write_install_metadata @ hermes_cli/plugins_cmd.py:_write_install_metadata */
static int p_write_install_metadata(const json_t *meta) {
    char home[1024], path[2048], tmp[2200];
    p_home(home, sizeof home);
    snprintf(path, sizeof path, "%s/plugins/%s", home, INSTALL_METADATA_FILE);
    char *text = json_dumps(meta, 2);
    if (!text) return -1;
    snprintf(tmp, sizeof tmp, "%s.tmp-%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(text); return -1; }
    fputs(text, f);
    fputc('\n', f);
    fclose(f);
    free(text);
    return rename(tmp, path);
}

/* ------------------------------------------------------------------ */
/* cmd_install                                                         */
/* ------------------------------------------------------------------ */

/* PoP: cmd_install @ hermes_cli/plugins_cmd.py:cmd_install */
void cmd_install(const char *identifier, int force, int enable, const char *ref) {
    if (!identifier || !*identifier) {
        fprintf(stderr, "Error: an install identifier is required.\n");
        exit(1);
    }
    char ident[1024];
    snprintf(ident, sizeof ident, "%s", identifier);

    /* Bare names resolve through the community plugin index to owner/repo
     * plus the index-pinned ref; an explicit --ref always wins over the pin. */
    if (!strchr(ident, '/') && !strstr(ident, "://") &&
        strncmp(ident, "git@", 4) != 0) {
        char *index_json = p_load_index_json(0);
        char *resolved = index_json ? pindex_resolve_name(index_json, ident) : NULL;
        if (resolved && *resolved) {
            json_t *rv = json_parse(resolved, NULL);
            if (rv && rv->type == JSON_OBJECT) {
                const json_t *id = json_obj_get(rv, "identifier");
                if (id && id->type == JSON_STRING)
                    snprintf(ident, sizeof ident, "%s", id->str_val);
                const json_t *pin = json_obj_get(rv, "ref");
                static char pinned_ref[128];
                if ((ref == NULL || !*ref) && pin && pin->type == JSON_STRING &&
                    pin->str_val[0]) {
                    snprintf(pinned_ref, sizeof pinned_ref, "%s", pin->str_val);
                    ref = pinned_ref;
                }
                json_free(rv);
            } else if (rv) {
                json_free(rv);
            }
        }
        free(resolved);
        free(index_json);
    }

    if (ref && *ref && !plugin_pack_is_exact_sha(ref)) {
        fprintf(stderr, "Error: --ref must be a full 40-character commit SHA.\n");
        exit(1);
    }

    char *subdir = NULL;
    char *git_url = pcmd_resolve_git_url(ident, &subdir);
    if (!git_url) {
        fprintf(stderr, "Error: cannot resolve '%s' to a git URL.\n", ident);
        exit(1);
    }
    if (strncmp(git_url, "http://", 7) == 0 || strncmp(git_url, "file://", 7) == 0)
        printf("Warning: Using insecure/local URL scheme. Consider using "
               "https:// or git@ for production installs.\n");
    if (subdir && *subdir)
        printf("Cloning %s (subdir: %s)...\n", git_url, subdir);
    else
        printf("Cloning %s...\n", git_url);

    char flat[512], target[2048], home[1024];
    p_home(home, sizeof home);
    p_flat_install_dir(ident, flat, sizeof flat);
    snprintf(target, sizeof target, "%s/plugins/%s", home, flat);

    if (p_is_dir(target)) {
        if (!force) {
            fprintf(stderr,
                    "Error: plugin directory '%s' already exists "
                    "(use --force to reinstall).\n",
                    flat);
            free(git_url);
            free(subdir);
            exit(1);
        }
        p_rm_rf(target);
    }

    int rc = p_git_clone_into(git_url, flat, target, sizeof target);
    free(git_url);
    free(subdir);
    if (rc != 0 || !p_is_dir(target)) {
        fprintf(stderr, "Error: git clone failed for '%s'.\n", ident);
        exit(1);
    }

    if (!p_child_exists(target, "plugin.yaml") &&
        !p_child_exists(target, "plugin.yml") &&
        !p_child_exists(target, "plugin.json") &&
        !p_child_exists(target, "__init__.py"))
        printf("Warning: %s doesn't contain plugin.yaml, plugin.json, or "
               "__init__.py. It may not be a valid Hermes plugin.\n", flat);

    int should_enable = enable;
    if (should_enable == PCMD_ENABLE_PROMPT) {
        if (isatty(0) && isatty(1)) {
            char q[512], ans[32];
            snprintf(q, sizeof q, "  Enable '%s' now? [y/N]: ", flat);
            p_prompt_line(q, ans, sizeof ans);
            should_enable = p_answer_yes(ans) ? 1 : 0;
        } else {
            should_enable = 0;
        }
    }
    if (should_enable == 1) {
        strset_t en, dis;
        p_get_name_set("enabled", &en);
        p_get_name_set("disabled", &dis);
        strset_add(&en, flat);
        strset_discard(&dis, flat);
        p_save_name_set("enabled", &en);
        p_save_name_set("disabled", &dis);
        strset_free(&en);
        strset_free(&dis);
        printf("\xe2\x9c\x93 Plugin %s enabled.\n", flat);
    } else {
        printf("Plugin installed but not enabled. Run `hermes plugins enable %s` "
               "to activate.\n", flat);
    }

    printf("Restart the gateway for the plugin to take effect:\n");
    printf("  hermes gateway restart\n\n");
}

/* ------------------------------------------------------------------ */
/* cmd_remove                                                          */
/* ------------------------------------------------------------------ */

/* PoP: cmd_remove @ hermes_cli/plugins_cmd.py:cmd_remove */
void cmd_remove(const char *name) {
    if (!name || !*name) {
        fprintf(stderr, "Error: plugin name required\n");
        exit(1);
    }
    char home[1024], plugins_dir[1200], target[2048];
    p_home(home, sizeof home);
    snprintf(plugins_dir, sizeof plugins_dir, "%s/plugins", home);

    /* Accept bare dir names and nested category/name keys. */
    snprintf(target, sizeof target, "%s/%s", plugins_dir, name);
    if (!p_is_dir(target)) {
        char sub[2048];
        snprintf(sub, sizeof sub, "%s/plugins", home);
        DIR *d = opendir(sub);
        int found = 0;
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.') continue;
                snprintf(target, sizeof target, "%s/plugins/%s/%s", home,
                         e->d_name, name);
                if (p_is_dir(target)) { found = 1; break; }
            }
            closedir(d);
        }
        if (!found) {
            fprintf(stderr, "Error: Plugin '%s' not found in %s/plugins.\n",
                    name, home);
            exit(1);
        }
    }

    /* _remove_plugin_core: swap the tree out through a staging dir so the
     * metadata write is all-or-nothing. */
    json_t *meta = p_read_install_metadata();
    int in_meta = json_obj_get(meta, name) != NULL ||
                  strstr(name, "/"); /* nested keys stored by basename below */
    const char *meta_key = strrchr(name, '/');
    meta_key = meta_key ? meta_key + 1 : name;
    if (json_obj_get(meta, meta_key)) in_meta = 1;

    char staging_tpl[2304], staging[2400];
    snprintf(staging_tpl, sizeof staging_tpl, "%s/.%s.remove-XXXXXX", plugins_dir,
             meta_key);
    if (!mkdtemp(staging_tpl)) {
        fprintf(stderr, "Error: Could not remove plugin '%s': staging failed.\n",
                name);
        json_free(meta);
        exit(1);
    }
    snprintf(staging, sizeof staging, "%s/plugin", staging_tpl);

    if (rename(target, staging) != 0) {
        fprintf(stderr, "Error: Could not remove plugin '%s'.\n", name);
        rmdir(staging_tpl);
        json_free(meta);
        exit(1);
    }
    if (in_meta) {
        json_obj_del(meta, meta_key);
        if (p_write_install_metadata(meta) != 0) {
            rename(staging, target); /* restore */
            fprintf(stderr,
                    "Error: Could not remove plugin '%s': metadata update "
                    "failed; recovery copy remains at %s.\n",
                    name, staging);
            json_free(meta);
            exit(1);
        }
    }
    json_free(meta);
    p_rm_rf(staging_tpl);

    printf("Removed plugin '%s'\n", name);
    printf("Restart the gateway for the change to take effect:\n");
    printf("  hermes gateway restart\n");
}

/* ------------------------------------------------------------------ */
/* cmd_enable / cmd_disable                                            */
/* ------------------------------------------------------------------ */

/* PoP: cmd_enable @ hermes_cli/plugins_cmd.py:cmd_enable */
void cmd_enable(const char *name, int allow_tool_override) {
    if (!name || !*name) {
        fprintf(stderr, "Error: plugin name required\n");
        exit(1);
    }
    if (p_is_legacy_relay_key(name)) {
        fprintf(stderr,
                "Plugin '%s' was removed. Relay lifecycle is owned by Hermes "
                "core; configure %s instead.\n", name, RELAY_PLUGINS_CONFIG_ENV);
        exit(1);
    }

    char key[1024], source[64];
    if (!p_resolve_key_and_source(name, key, sizeof key, source, sizeof source)) {
        fprintf(stderr, "Plugin '%s' is not installed or bundled.\n", name);
        exit(1);
    }
    if (p_is_legacy_relay_key(key)) {
        fprintf(stderr,
                "Plugin '%s' was removed. Relay lifecycle is owned by Hermes "
                "core; configure %s instead.\n", key, RELAY_PLUGINS_CONFIG_ENV);
        exit(1);
    }

    strset_t enabled, disabled;
    p_get_name_set("enabled", &enabled);
    p_get_name_set("disabled", &disabled);

    int already_enabled = strset_contains(&enabled, key) &&
                          !strset_contains(&disabled, key);

    if (!already_enabled) {
        strset_add(&enabled, key);
        strset_discard(&disabled, key);
        /* Drop every alias (bare leaf + manifest name) from the disabled list
         * so a stale entry can't keep the plugin vetoed (#40190 follow-up). */
        const char *slash = strrchr(key, '/');
        const char *bare = slash ? slash + 1 : key;
        if (strcmp(bare, key) != 0) strset_discard(&disabled, bare);
        /* manifest-name alias */
        plugins_scan_result_t *r = plugins_collect_directory_manifests();
        for (size_t i = 0; i < plugins_scan_count(r); i++) {
            const char *k = plugins_scan_key(r, i);
            if (k && strcmp(k, key) == 0) {
                const char *nm = plugins_scan_name(r, i);
                if (nm) strset_discard(&disabled, nm);
                break;
            }
        }
        plugins_scan_free(r);

        p_save_name_set("enabled", &enabled);
        p_save_name_set("disabled", &disabled);
        printf("\xe2\x9c\x93 Plugin %s enabled. Takes effect on next session.\n",
               key);
    } else {
        printf("Plugin '%s' is already enabled.\n", key);
    }

    if (strcmp(source, "bundled") == 0) {
        strset_free(&enabled);
        strset_free(&disabled);
        return;
    }

    /* allow_tool_override tri-state resolution (consent screen boundary:
     * interactive prompt with deny default; explicit flags skip the prompt). */
    int grant = allow_tool_override;
    if (grant == PCMD_ENABLE_PROMPT) {
        if (isatty(0) && isatty(1)) {
            printf("Allow this plugin to replace built-in tools "
                   "(e.g. shell_exec, write_file)?\n");
            printf("  This is a privileged capability: an override can "
                   "intercept everything the agent routes through that tool.\n");
            char ans[32];
            p_prompt_line("  Grant it? [y/N] ", ans, sizeof ans);
            grant = p_answer_yes(ans) ? 1 : 0;
        } else {
            grant = 0;
        }
    }
    char dotted[1200];
    snprintf(dotted, sizeof dotted, "plugins.entries.%s.allow_tool_override", key);
    json_t *bv = json_bool(grant != 0);
    int rc = config_py_save_value(dotted, bv);
    json_free(bv);
    if (rc == 0) {
        if (grant)
            printf("\xe2\x9c\x93 Granted %s permission to override built-in "
                   "tools (plugins.entries.%s.allow_tool_override: true).\n",
                   key, key);
        else
            printf("%s may not override built-in tools. Re-run `hermes plugins "
                   "enable %s --allow-tool-override` to grant this later.\n",
                   key, key);
    }
    strset_free(&enabled);
    strset_free(&disabled);
}

/* PoP: cmd_disable @ hermes_cli/plugins_cmd.py:cmd_disable */
void cmd_disable(const char *name) {
    if (!name || !*name) {
        fprintf(stderr, "Error: plugin name required\n");
        exit(1);
    }
    char key[1024], source[64];
    if (!p_resolve_key_and_source(name, key, sizeof key, source, sizeof source)) {
        fprintf(stderr, "Plugin '%s' is not installed or bundled.\n", name);
        exit(1);
    }

    strset_t enabled, disabled;
    p_get_name_set("enabled", &enabled);
    p_get_name_set("disabled", &disabled);

    if (!strset_contains(&enabled, key) && strset_contains(&disabled, key)) {
        printf("Plugin '%s' is already disabled.\n", key);
        strset_free(&enabled);
        strset_free(&disabled);
        return;
    }

    strset_discard(&enabled, key);
    /* Drop any legacy bare-name entry from the allow-list too. */
    const char *slash = strrchr(key, '/');
    const char *bare = slash ? slash + 1 : key;
    if (strcmp(bare, key) != 0) strset_discard(&enabled, bare);
    strset_add(&disabled, key);
    p_save_name_set("enabled", &enabled);
    p_save_name_set("disabled", &disabled);
    printf("\xe2\x8a\x98 Plugin %s disabled. Takes effect on next session.\n",
           key);
    strset_free(&enabled);
    strset_free(&disabled);
}

/* ------------------------------------------------------------------ */
/* cmd_list                                                            */
/* ------------------------------------------------------------------ */

/* PoP: cmd_list @ hermes_cli/plugins_cmd.py:cmd_list */
void cmd_list(int opt_json, int opt_plain, int opt_no_bundled_or_user,
              int opt_enabled_only) {
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    size_t n = plugins_scan_count(r);

    if (n == 0) {
        printf("No plugins installed.\n");
        printf("Install with: hermes plugins install owner/repo\n");
        plugins_scan_free(r);
        return;
    }

    strset_t enabled, disabled;
    p_get_name_set("enabled", &enabled);
    p_get_name_set("disabled", &disabled);

    /* _filter_plugin_entries */
    typedef struct { const char *key, *nm, *src, *path; } row_t;
    row_t *rows = malloc(n * sizeof(row_t));
    size_t nr = 0;
    for (size_t i = 0; i < n; i++) {
        const char *src = plugins_scan_source(r, i);
        if (opt_no_bundled_or_user && src && strcmp(src, "bundled") == 0)
            continue;
        char st[32];
        p_status_of(plugins_scan_name(r, i), plugins_scan_key(r, i), &enabled,
                    &disabled, st, sizeof st);
        if (opt_enabled_only && strcmp(st, "enabled") != 0)
            continue;
        rows[nr].key = plugins_scan_key(r, i);
        rows[nr].nm = plugins_scan_name(r, i);
        rows[nr].src = src;
        rows[nr].path = plugins_scan_path(r, i);
        nr++;
    }

    if (nr == 0) {
        if (opt_json) printf("[]\n");
        else printf("No plugins matched the selected filters.\n");
        goto done;
    }

    if (opt_json) {
        json_t *payload = json_array();
        for (size_t i = 0; i < nr; i++) {
            char st[32];
            p_status_of(rows[i].nm, rows[i].key, &enabled, &disabled, st,
                        sizeof st);
            json_t *o = json_object();
            json_t *manifest = p_manifest_read(rows[i].path);
            const char *ver = manifest ? p_json_str(manifest, "version", "") : "";
            const char *desc = manifest ? p_json_str(manifest, "description", "") : "";
            json_set(o, "name", json_string(rows[i].nm));
            json_set(o, "status", json_string(st));
            json_set(o, "version", json_string(ver));
            json_set(o, "description", json_string(desc));
            json_set(o, "source", json_string(rows[i].src));
            json_append(payload, o);
            if (manifest) json_free(manifest);
        }
        char *s = json_dumps(payload, 2);
        printf("%s\n", s ? s : "[]");
        free(s);
        json_free(payload);
        goto done;
    }

    if (opt_plain) {
        for (size_t i = 0; i < nr; i++) {
            char st[32];
            p_status_of(rows[i].nm, rows[i].key, &enabled, &disabled, st,
                        sizeof st);
            json_t *manifest = p_manifest_read(rows[i].path);
            const char *ver = manifest ? p_json_str(manifest, "version", "") : "";
            printf("%-12s %-8s %-8s %s\n", st, rows[i].src, ver, rows[i].nm);
            if (manifest) json_free(manifest);
        }
        goto done;
    }

    /* table view */
    printf("\n");
    printf("%-28s %-12s %-10s %-24s %s\n", "Name", "Status", "Version",
           "Description", "Source");
    for (size_t i = 0; i < nr; i++) {
        char st[32];
        p_status_of(rows[i].nm, rows[i].key, &enabled, &disabled, st,
                    sizeof st);
        json_t *manifest = p_manifest_read(rows[i].path);
        const char *ver = manifest ? p_json_str(manifest, "version", "") : "";
        const char *desc = manifest ? p_json_str(manifest, "description", "") : "";
        char desc_trunc[48];
        snprintf(desc_trunc, sizeof desc_trunc, "%s", desc);
        printf("%-28s %-12s %-10s %-24s %s\n", rows[i].nm, st, ver, desc_trunc,
               rows[i].src);
        if (manifest) json_free(manifest);
    }
    printf("\n");
    printf("Compact view: hermes plugins list --plain --no-bundled\n");
    printf("Interactive toggle: hermes plugins\n");
    printf("Enable/disable: hermes plugins enable/disable <name>\n");
    printf("Plugins are opt-in by default — only 'enabled' plugins load.\n");

done:
    free(rows);
    plugins_scan_free(r);
    strset_free(&enabled);
    strset_free(&disabled);
}

/* ------------------------------------------------------------------ */
/* cmd_capabilities                                                    */
/* ------------------------------------------------------------------ */

static size_t p_caps_declared(const char *dir, const char ***out_caps) {
    static const char *caps_buf[64];
    static char caps_store[64][128];
    size_t ncaps = 0;
    *out_caps = NULL;
    json_t *m = dir ? p_manifest_read(dir) : NULL;
    const json_t *caps = m ? json_obj_get(m, "capabilities") : NULL;
    if (caps && caps->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)caps) && ncaps < 64; i++) {
            const json_t *v = json_get((json_t *)caps, i);
            if (v && v->type == JSON_STRING) {
                snprintf(caps_store[ncaps], sizeof caps_store[0], "%s",
                         v->str_val);
                caps_buf[ncaps++] = caps_store[ncaps - 1];
            }
        }
    }
    if (m) json_free(m);
    *out_caps = caps_buf;
    return ncaps;
}

/* PoP: cmd_capabilities @ hermes_cli/plugins_cmd.py:cmd_capabilities */
void cmd_capabilities(const char *name) {
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    size_t n = plugins_scan_count(r);
    int printed = 0;

    for (size_t i = 0; i < n; i++) {
        const char *key = plugins_scan_key(r, i);
        const char *nm = plugins_scan_name(r, i);
        if (name && *name && strcmp(key ? key : "", name) != 0 &&
            strcmp(nm ? nm : "", name) != 0)
            continue;
        const char *eff_key = (key && *key) ? key : nm;
        const char **caps = NULL;
        size_t ncaps = p_caps_declared(plugins_scan_path(r, i), &caps);

        /* granted set from plugins.entries.<id>.granted_capabilities */
        strset_t granted;
        memset(&granted, 0, sizeof granted);
        {
            char dotted[1300];
            snprintf(dotted, sizeof dotted,
                     "plugins.entries.%s.granted_capabilities", eff_key);
            json_t *cfg = p_cfg_load();
            json_t *gv = cfg ? config_py_get_nested(cfg, dotted) : NULL;
            strset_from_json(&granted, gv);
            if (cfg) json_free(cfg);
        }

        if (!ncaps && granted.n == 0 && !name)
            continue;

        printf("%s (%s)\n", eff_key, plugins_scan_source(r, i));
        if (ncaps == 0) printf("  declared: (none)\n");
        for (size_t c = 0; c < ncaps; c++)
            printf("  %s: %s%s%s\n", caps[c],
                   strset_contains(&granted, caps[c]) ? "granted" : "not granted",
                   (strset_contains(&granted, caps[c]) &&
                    !strset_contains(&granted, caps[c]))
                       ? "" : "",
                   "");
        strset_free(&granted);
        printed++;
    }
    plugins_scan_free(r);

    if (name && *name && !printed) {
        fprintf(stderr, "Plugin '%s' is not installed or bundled.\n", name);
        exit(1);
    }
    if (!printed) printf("No plugins declare or hold capabilities.\n");
}

/* ------------------------------------------------------------------ */
/* cmd_toggle                                                          */
/* ------------------------------------------------------------------ */

/*
 * Text-mode path of the composite UI (_run_composite_fallback semantics):
 * numbered toggling persisted by canonical key. The curses screen is the
 * PoP: cmd_toggle @ hermes_cli/plugins_cmd.py:cmd_toggle
 */
void cmd_toggle(void) {
    if (!isatty(0)) {
        printf("Interactive mode requires a terminal.\n");
        return;
    }

    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    size_t n = plugins_scan_count(r);
    strset_t enabled, disabled;
    p_get_name_set("enabled", &enabled);
    p_get_name_set("disabled", &disabled);

    if (n == 0) {
        printf("No plugins installed and no provider categories available.\n");
        printf("Install with: hermes plugins install owner/repo\n");
        plugins_scan_free(r);
        strset_free(&enabled);
        strset_free(&disabled);
        return;
    }

    /* initial selection state per canonical key */
    unsigned char *chosen = calloc(n, 1);
    const char **keys = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) {
        keys[i] = plugins_scan_key(r, i);
        const char *nm = plugins_scan_name(r, i);
        if ((strset_contains(&enabled, keys[i]) ||
             strset_contains(&enabled, nm)) &&
            !strset_contains(&disabled, keys[i]) &&
            !strset_contains(&disabled, nm))
            chosen[i] = 1;
    }

    printf("\n  Plugins\n\n  General Plugins\n");
    printf("  Toggle by number, Enter to confirm.\n\n");
    for (;;) {
        for (size_t i = 0; i < n; i++) {
            const char *desc = "";
            json_t *m = p_manifest_read(plugins_scan_path(r, i));
            if (m) desc = p_json_str(m, "description", "");
            char label[256];
            if (desc && *desc)
                snprintf(label, sizeof label, "%s — %s", plugins_scan_name(r, i),
                         desc);
            else
                snprintf(label, sizeof label, "%s", plugins_scan_name(r, i));
            if (m) json_free(m);
            printf("  [%s] %3zu. %s%s\n", chosen[i] ? "\xe2\x9c\x93" : " ",
                   i + 1, label,
                   plugins_scan_source(r, i) &&
                           strcmp(plugins_scan_source(r, i), "bundled") == 0
                       ? " [bundled]"
                       : "");
        }
        printf("\n  Toggle # (or Enter to confirm): ");
        fflush(stdout);
        char buf[64];
        if (!fgets(buf, sizeof buf, stdin)) break;
        size_t L = strlen(buf);
        while (L && (buf[L - 1] == '\n' || buf[L - 1] == '\r')) buf[--L] = '\0';
        if (L == 0) break;
        char *end = NULL;
        long idx = strtol(buf, &end, 10) - 1;
        if (idx >= 0 && (size_t)idx < n) chosen[idx] = !chosen[idx];
        printf("\n");
    }

    /* Persist by canonical key only — never the bare manifest name (#40190). */
    strset_t new_enabled, new_disabled;
    memset(&new_enabled, 0, sizeof new_enabled);
    new_disabled = (strset_t){NULL, 0, 0};
    for (size_t i = 0; i < disabled.n; i++) strset_push(&new_disabled, disabled.v[i]);
    for (size_t i = 0; i < n; i++) {
        const char *slash = strrchr(keys[i], '/');
        const char *bare = slash ? slash + 1 : keys[i];
        if (chosen[i]) {
            strset_add(&new_enabled, keys[i]);
            strset_discard(&new_disabled, keys[i]);
            if (strcmp(bare, keys[i]) != 0) strset_discard(&new_disabled, bare);
        } else {
            strset_add(&new_disabled, keys[i]);
        }
    }

    int changed = (new_enabled.n != enabled.n) ||
                  (new_disabled.n != disabled.n);
    for (size_t i = 0; i < new_enabled.n && !changed; i++)
        if (!strset_contains(&enabled, new_enabled.v[i])) changed = 1;
    for (size_t i = 0; i < new_disabled.n && !changed; i++)
        if (!strset_contains(&disabled, new_disabled.v[i])) changed = 1;

    if (changed) {
        p_save_name_set("enabled", &new_enabled);
        p_save_name_set("disabled", &new_disabled);
        printf("\n\xe2\x9c\x93 General plugins: %zu enabled, %zu disabled.\n",
               new_enabled.n, n - new_enabled.n);
    } else if (n > 0) {
        printf("\nGeneral plugins unchanged.\n");
    }
    printf("Changes take effect on next session.\n\n");

    free(chosen);
    free(keys);
    plugins_scan_free(r);
    strset_free(&enabled);
    strset_free(&disabled);
    strset_free(&new_enabled);
    strset_free(&new_disabled);
}

/* ------------------------------------------------------------------ */
/* cmd_plugin_doctor                                                   */
/* ------------------------------------------------------------------ */

/*
 * doctor_plugin() service boundary: the C side performs the discovery +
 * PoP: cmd_plugin_doctor @ hermes_cli/plugins_cmd.py:cmd_plugin_doctor
 */
void cmd_plugin_doctor(const char *target, int ci) {
    if (!target || !*target) target = ".";
    if (!p_is_dir(target)) {
        printf("error: target directory not found: %s\n", target);
        if (ci) exit(1);
        return;
    }
    int ok = 1;
    printf("Doctor report for %s\n", target);
    json_t *m = p_manifest_read(target);
    if (!m) {
        printf("  [FAIL] no plugin.yaml / plugin.yml / plugin.json manifest\n");
        ok = 0;
    } else {
        printf("  [ OK ] manifest found (name: %s)\n",
               p_json_str(m, "name", "?"));
        const json_t *ep = json_obj_get(m, "entry_point");
        if (!ep) ep = json_obj_get(m, "entrypoint");
        if (!ep)
            printf("  [WARN] no entry point declared\n");
        else
            printf("  [ OK ] entry point declared\n");
        json_free(m);
    }
    printf("  result: %s\n", ok ? "ok" : "failed");
    if (ci && !ok) exit(1);
}

/* ------------------------------------------------------------------ */
/* cmd_search                                                          */
/* ------------------------------------------------------------------ */

/* PoP: cmd_search @ hermes_cli/plugins_cmd.py:cmd_search */
#define SECURITY_FOOTER                                                        \
    "Indexed \xe2\x89\xa0 audited: inclusion in the index is a metadata review " \
    "only, not a code audit. Review a plugin before enabling it."

void cmd_search(const char *term, int json_output, const char *capability,
                int refresh) {
    char *index_json = p_load_index_json(refresh);
    if (!index_json) {
        fprintf(stderr, "Error: plugin index unavailable (no cache or seed).\n");
        exit(1);
    }
    char *results = pindex_search_index(index_json, term ? term : "", capability);
    free(index_json);
    if (!results) results = strdup("[]");

    if (json_output) {
        json_t *arr = json_parse(results, NULL);
        json_t *doc = json_object();
        json_set(doc, "source", json_string("cache"));
        json_set(doc, "query", json_string(term ? term : ""));
        json_set(doc, "results", arr ? arr : json_array());
        json_set(doc, "note", json_string(SECURITY_FOOTER));
        char *s = json_dumps(doc, 2);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(doc);
    } else {
        json_t *arr = json_parse(results, NULL);
        size_t cnt = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
        if (cnt == 0) {
            printf("No plugins matched '%s' (index source: cache)\n",
                   term ? term : "");
        } else {
            printf("Community plugins (%zu match%s)\n", cnt,
                   cnt == 1 ? "" : "es");
            printf("%-24s %-52s %-16s %s\n", "Name", "Description", "Author",
                   "Tags");
            for (size_t i = 0; i < cnt; i++) {
                const json_t *e = json_get(arr, i);
                if (!e || e->type != JSON_OBJECT) continue;
                char desc[72];
                snprintf(desc, sizeof desc, "%s",
                         p_json_str(e, "description", ""));
                if (strlen(p_json_str(e, "description", "")) > 70)
                    strcpy(desc + 67, "...");
                /* tags */
                char tags[160];
                tags[0] = '\0';
                const json_t *tv = json_obj_get(e, "tags");
                size_t off = 0;
                if (tv && tv->type == JSON_ARRAY) {
                    for (size_t t = 0; t < json_len((json_t *)tv); t++) {
                        const json_t *tag = json_get((json_t *)tv, t);
                        if (tag && tag->type == JSON_STRING)
                            off += (size_t)snprintf(tags + off,
                                                    sizeof tags - off, "%s%s",
                                                    t ? ", " : "", tag->str_val);
                    }
                }
                printf("%-24s %-52s %-16s %s\n", p_json_str(e, "name", "?"),
                       desc, p_json_str(e, "author", ""), tags);
            }
            printf("Index source: cache. Install: hermes plugins install <name>\n");
            printf("%s\n", SECURITY_FOOTER);
        }
        if (arr) json_free(arr);
    }
    free(results);
}


/* ================================================================== */
/* plugin packs - hermes_cli/plugin_packs.py                           */
/* ================================================================== */

static const char *const PACK_RESERVED_ENTRY_KEYS[] = {
    "granted_capabilities", "capabilities_consent", NULL,
};

static int p_is_secret_key(const char *key) {
    /* _SECRET_KEY_RE (?i)(token|secret|passw(or)?d|api[_-]?key|
     * private[_-]?key|credential|auth) */
    static const char *const PATS[] = {
        "token", "secret", "passwd", "password", "api_key", "api-key",
        "private_key", "private-key", "credential", "auth", NULL,
    };
    if (!key) return 0;
    size_t klen = strlen(key);
    char *low = malloc(klen + 1);
    for (size_t i = 0; i < klen; i++)
        low[i] = (char)tolower((unsigned char)key[i]);
    low[klen] = '\0';
    int hit = 0;
    for (int i = 0; PATS[i] && !hit; i++)
        if (strstr(low, PATS[i])) hit = 1;
    free(low);
    return hit;
}

static int p_entry_reserved(const char *key) {
    for (int i = 0; PACK_RESERVED_ENTRY_KEYS[i]; i++)
        if (strcmp(key, PACK_RESERVED_ENTRY_KEYS[i]) == 0) return 1;
    return strncmp(key, "allow_", 6) == 0;
}

/* load_pack(): parse a local YAML/JSON pack file (URL fetch via curl). */
static json_t *p_pack_load(const char *source) {
    char *text = NULL;
    if (strncmp(source, "http://", 7) == 0 || strncmp(source, "https://", 8) == 0)
        text = p_fetch_url_curl(source);
    else {
        if (!p_exists(source)) return NULL;
        text = p_read_text(source);
    }
    if (!text) return NULL;
    json_t *doc = json_parse_yaml(text); /* handles JSON too */
    free(text);
    if (doc && doc->type != JSON_OBJECT) { json_free(doc); doc = NULL; }
    return doc;
}

/*
 * repo[/subdir] entries resolve directly; bare names go through the
 * PoP: resolve_pack_plugins @ hermes_cli/plugin_packs.py:resolve_pack_plugins
 */
json_t *resolve_pack_plugins(const json_t *pack) {
    json_t *out = json_array();
    const json_t *plugins = pack ? json_obj_get(pack, "plugins") : NULL;
    if (!plugins || plugins->type != JSON_ARRAY) return out;

    char *index_json = NULL;
    int index_loaded = 0;

    for (size_t i = 0; i < json_len((json_t *)plugins); i++) {
        const json_t *entry = json_get((json_t *)plugins, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        const char *name = p_json_str(entry, "name", "");
        const char *repo = p_json_str(entry, "repo", "");
        const char *subdir_v = p_json_str(entry, "subdir", NULL);
        const char *ref = p_json_str(entry, "ref", "");
        const char *display =
            (name && *name) ? name : ((repo && *repo) ? repo : "?");

        json_t *o = json_object();
        json_set(o, "display", json_string(display));
        json_set(o, "ref", json_string(ref));
        json_set(o, "identifier", json_null());
        json_set(o, "resolve_error", json_null());

        if (repo && *repo) {
            char ident[1024];
            if (subdir_v && *subdir_v)
                snprintf(ident, sizeof ident, "%s/%s", repo, subdir_v);
            else
                snprintf(ident, sizeof ident, "%s", repo);
            json_set(o, "identifier", json_string(ident));
        } else {
            /* bare community-index name */
            if (!index_loaded) {
                index_json = p_load_index_json(0);
                index_loaded = 1;
            }
            if (!index_json) {
                json_set(o, "resolve_error",
                         json_string("community index unavailable"));
            } else {
                char *rv = pindex_resolve_name(index_json, name);
                json_t *rjo = rv ? json_parse(rv, NULL) : NULL;
                if (rjo && rjo->type == JSON_OBJECT) {
                    const json_t *id = json_obj_get(rjo, "identifier");
                    if (id && id->type == JSON_STRING && id->str_val[0]) {
                        json_set(o, "identifier", json_string(id->str_val));
                        const json_t *caps = json_obj_get(rjo, "capabilities");
                        if (caps)
                            json_set(o, "capabilities",
                                     json_copy((json_t *)caps));
                    } else {
                        json_set(o, "resolve_error",
                                 json_string("not found in the community index"));
                    }
                    json_free(rjo);
                } else {
                    json_set(o, "resolve_error",
                             json_string("ambiguous in the community index"));
                }
                free(rv);
            }
        }
        json_append(out, o);
    }
    free(index_json);
    return out;
}

/* PoP: render_pack_review @ hermes_cli/plugin_packs.py:render_pack_review */
void render_pack_review(const json_t *pack, const json_t *resolved) {
    printf("\n%s", p_json_str(pack, "name", "?"));
    const char *ver = p_json_str(pack, "version", NULL);
    if (ver) printf(" v%s", ver);
    const char *author = p_json_str(pack, "author", NULL);
    if (author) printf(" - by %s", author);
    printf("\n");
    const char *desc = p_json_str(pack, "description", NULL);
    if (desc) printf("%s\n", desc);

    printf("  %-24s %-40s %-14s %s\n", "Plugin", "Source", "Pinned ref",
           "Capabilities (declared)");
    size_t nres = (resolved && resolved->type == JSON_ARRAY)
                      ? json_len((json_t *)resolved) : 0;
    for (size_t i = 0; i < nres; i++) {
        const json_t *rp = json_get((json_t *)resolved, i);
        const char *ident = p_json_str(rp, "identifier", NULL);
        const char *err = p_json_str(rp, "resolve_error", NULL);
        char src[256];
        snprintf(src, sizeof src, "%s", ident ? ident : "(unresolved)");
        printf("  %-24s %-40s %-14s %s\n", p_json_str(rp, "display", "?"),
               src, p_json_str(rp, "ref", ""), err ? err : "(shown at install)");
    }

    const json_t *cfg = pack ? json_obj_get(pack, "config") : NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        printf("Config seeds (plugins.entries.<id>, non-secret):\n");
        for (size_t k = 0; k < json_object_size(cfg); k++) {
            const char *pid = json_object_get_key_at(cfg, k);
            const json_t *seed = json_object_get_at(cfg, k);
            if (!seed || seed->type != JSON_OBJECT) continue;
            for (size_t j = 0; j < json_object_size(seed); j++) {
                const json_t *v = json_object_get_at(seed, j);
                char *vs = v ? json_dumps(v, 0) : NULL;
                printf("  %s.%s = %s\n", pid, json_object_get_key_at(seed, j),
                       vs ? vs : "null");
                free(vs);
            }
        }
    }
    const json_t *skills = pack ? json_obj_get(pack, "skills") : NULL;
    if (skills && skills->type == JSON_ARRAY &&
        json_len((json_t *)skills) > 0) {
        printf("Pack lists skills (NOT auto-installed yet): ");
        size_t off = 0;
        for (size_t s = 0; s < json_len((json_t *)skills); s++) {
            const json_t *sv = json_get((json_t *)skills, s);
            if (sv && sv->type == JSON_STRING)
                off += (size_t)printf("%s%s", s ? ", " : "", sv->str_val);
        }
        (void)off;
        printf("\n  Install them manually, e.g. `hermes skills install <id>`.\n");
    }
    printf("\nInstalling a pack runs third-party code x %zu plugins. Each "
           "plugin's declared capabilities still require individual consent "
           "after install - a pack never bulk-grants.\n", nres);
}

/*
 * Fan-out to ordinary pinned installs; per-plugin failures never abort.
 * The capability-consent screen is the interactive boundary: declared caps
 * PoP: install_pack_plugins @ hermes_cli/plugin_packs.py:install_pack_plugins
 */
json_t *install_pack_plugins(const json_t *pack, const json_t *resolved,
                             int force) {
    json_t *results = json_array();
    size_t nres = (resolved && resolved->type == JSON_ARRAY)
                      ? json_len((json_t *)resolved) : 0;
    for (size_t i = 0; i < nres; i++) {
        const json_t *rp = json_get((json_t *)resolved, i);
        const char *display = p_json_str(rp, "display", "?");
        const char *ident = p_json_str(rp, "identifier", NULL);
        const char *ref = p_json_str(rp, "ref", "");

        json_t *o = json_object();
        json_set(o, "display", json_string(display));
        json_set(o, "ok", json_bool(0));

        if (!ident) {
            const char *err = p_json_str(rp, "resolve_error", "unresolved");
            json_set(o, "error", json_string(err));
            json_append(results, o);
            printf("\xe2\x9c\x97 %s: %s\n", display, err);
            continue;
        }

        printf("Installing %s @ %.12s...\n", display, ref);
        char *subdir = NULL;
        char *git_url = pcmd_resolve_git_url(ident, &subdir);
        if (!git_url) {
            json_set(o, "error", json_string("cannot resolve to git URL"));
            json_append(results, o);
            printf("\xe2\x9c\x97 %s: cannot resolve to git URL\n", display);
            continue;
        }
        char flat[512], target[2048], home[1024];
        p_home(home, sizeof home);
        p_flat_install_dir(ident, flat, sizeof flat);
        snprintf(target, sizeof target, "%s/plugins/%s", home, flat);
        if (p_is_dir(target)) {
            if (!force) {
                json_set(o, "error", json_string("already installed"));
                json_append(results, o);
                printf("\xe2\x9c\x97 %s: already installed\n", display);
                free(git_url);
                free(subdir);
                continue;
            }
            p_rm_rf(target);
        }
        if (p_git_clone_into(git_url, flat, target, sizeof target) != 0) {
            json_set(o, "error", json_string("git clone failed"));
            json_append(results, o);
            printf("\xe2\x9c\x97 %s: git clone failed\n", display);
            free(git_url);
            free(subdir);
            continue;
        }
        free(git_url);
        free(subdir);

        /* Enable — the user confirmed the mandatory review screen. */
        strset_t en, dis;
        p_get_name_set("enabled", &en);
        p_get_name_set("disabled", &dis);
        strset_add(&en, flat);
        strset_discard(&dis, flat);
        p_save_name_set("enabled", &en);
        p_save_name_set("disabled", &dis);
        strset_free(&en);
        strset_free(&dis);

        json_t *manifest = p_manifest_read(target);
        if (manifest) {
            const json_t *caps = json_obj_get(manifest, "capabilities");
            if (caps && caps->type == JSON_ARRAY &&
                json_len((json_t *)caps) > 0) {
                printf("  %s requests capabilities:", flat);
                for (size_t c = 0; c < json_len((json_t *)caps); c++) {
                    const json_t *cv = json_get((json_t *)caps, c);
                    if (cv && cv->type == JSON_STRING)
                        printf(" %s", cv->str_val);
                }
                printf("\n  Grant interactively with: hermes plugins enable %s\n",
                       flat);
            }
            json_free(manifest);
        }

        /* Pack config seed: fill only blanks (existing user values win). */
        const json_t *cfg = pack ? json_obj_get(pack, "config") : NULL;
        const json_t *seed = (cfg && cfg->type == JSON_OBJECT)
                                 ? json_obj_get(cfg, flat) : NULL;
        if (cfg && cfg->type == JSON_OBJECT && !seed)
            seed = json_obj_get(cfg, p_json_str(rp, "display", ""));
        if (seed && seed->type == JSON_OBJECT) {
            json_t *config = p_cfg_load();
            for (size_t k = 0; k < json_object_size(seed); k++) {
                const char *sk = json_object_get_key_at(seed, k);
                if (p_entry_reserved(sk) || p_is_secret_key(sk)) continue;
                const json_t *plug =
                    config ? json_obj_get(config, "plugins") : NULL;
                const json_t *entries =
                    (plug && plug->type == JSON_OBJECT)
                        ? json_obj_get(plug, "entries") : NULL;
                const json_t *cur =
                    (entries && entries->type == JSON_OBJECT)
                        ? json_obj_get(entries, flat) : NULL;
                if (cur && cur->type == JSON_OBJECT && json_obj_get(cur, sk)) {
                    printf("  plugins.entries.%s.%s already set - keeping "
                           "your value.\n", flat, sk);
                    continue;
                }
                char dotted[1600];
                snprintf(dotted, sizeof dotted, "plugins.entries.%s.%s", flat,
                         sk);
                config_py_save_value(
                    dotted, json_copy((json_t *)json_object_get_at(seed, k)));
            }
            printf("  Seeded plugins.entries.%s from pack.\n", flat);
            if (config) json_free(config);
        }

        json_set(o, "ok", json_bool(1));
        json_set(o, "installed_name", json_string(flat));
        json_append(results, o);
        printf("\xe2\x9c\x93 %s installed as %s.\n", display, flat);
    }
    return results;
}

/* PoP: cmd_pack_show @ hermes_cli/plugin_packs.py:cmd_pack_show */
void cmd_pack_show(const char *source) {
    if (!source || !*source) {
        fprintf(stderr, "Error: pack source required\n");
        exit(1);
    }
    json_t *pack = p_pack_load(source);
    if (!pack) {
        fprintf(stderr, "Error: could not load pack '%s'.\n", source);
        exit(1);
    }
    json_t *resolved = resolve_pack_plugins(pack);
    render_pack_review(pack, resolved);

    size_t unresolved = 0;
    for (size_t i = 0; i < json_len(resolved); i++) {
        const json_t *rp = json_get(resolved, i);
        const json_t *idv = rp ? json_obj_get(rp, "identifier") : NULL;
        if (!idv || idv->type == JSON_NULL) unresolved++;
    }
    if (unresolved)
        printf("\n%zu entr%s could not be resolved - install would skip them "
               "and exit non-zero.\n", unresolved, unresolved == 1 ? "y" : "ies");
    printf("\nDry run only. Install with `hermes plugins pack install ...`.\n");
    json_free(resolved);
    json_free(pack);
}

/* PoP: cmd_pack_install @ hermes_cli/plugin_packs.py:cmd_pack_install */
void cmd_pack_install(const char *source, int force) {
    if (!source || !*source) {
        fprintf(stderr, "Error: pack source required\n");
        exit(1);
    }
    json_t *pack = p_pack_load(source);
    if (!pack) {
        fprintf(stderr, "Error: could not load pack '%s'.\n", source);
        exit(1);
    }
    json_t *resolved = resolve_pack_plugins(pack);
    render_pack_review(pack, resolved);

    /* Mandatory review confirmation - no --yes in v1. Non-interactive
     * sessions cannot consent. */
    if (!(isatty(0) && isatty(1))) {
        fprintf(stderr,
                "Error: Pack install requires an interactive terminal to "
                "review and confirm the pack contents (no --yes in v1).\n");
        exit(1);
    }
    char prompt[512], ans[32];
    snprintf(prompt, sizeof prompt,
             "\nInstall %zu plugin(s) from pack '%s'? [y/N] ",
             json_len(resolved), p_json_str(pack, "name", "?"));
    p_prompt_line(prompt, ans, sizeof ans);
    if (!p_answer_yes(ans)) {
        printf("Aborted - nothing installed.\n");
        exit(1);
    }

    json_t *results = install_pack_plugins(pack, resolved, force);
    size_t ok_n = 0, failed_n = 0;
    for (size_t i = 0; i < json_len(results); i++) {
        const json_t *o = json_get(results, i);
        const json_t *okv = o ? json_obj_get(o, "ok") : NULL;
        if (okv && okv->bool_val)
            ok_n++;
        else
            failed_n++;
    }
    printf("\nPack '%s': %zu installed, %zu failed.\n",
           p_json_str(pack, "name", "?"), ok_n, failed_n);
    for (size_t i = 0; i < json_len(results); i++) {
        const json_t *o = json_get(results, i);
        const json_t *okv = o ? json_obj_get(o, "ok") : NULL;
        if (okv && !okv->bool_val)
            printf("  \xe2\x9c\x97 %s: %s\n", p_json_str(o, "display", "?"),
                   p_json_str(o, "error", "?"));
    }
    if (ok_n) {
        const char *a = "hermes", *b = "gateway", *c = "restart";
        printf("Restart the %s for the plugins to take effect:\n", b);
        printf("  %s %s %s\n", a, b, c);
    }
    json_free(results);
    json_free(resolved);
    json_free(pack);
    if (failed_n) exit(1);
}

/* minimal YAML emitter for the export document shape */
static void p_yaml_emit(FILE *f, const json_t *v, int indent);

static void p_yaml_emit_scalar(FILE *f, const json_t *v) {
    switch (v ? v->type : JSON_NULL) {
    case JSON_STRING:
        fprintf(f, "%s\n", v->str_val);
        break;
    case JSON_BOOL:
        fprintf(f, "%s\n", v->bool_val ? "true" : "false");
        break;
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d)
            fprintf(f, "%lld\n", (long long)d);
        else
            fprintf(f, "%g\n", d);
        break;
    }
    default:
        fprintf(f, "null\n");
        break;
    }
}

static void p_yaml_emit(FILE *f, const json_t *v, int indent) {
    if (v && v->type == JSON_OBJECT) {
        fputc('\n', f);
        for (size_t i = 0; i < json_object_size(v); i++) {
            const json_t *item = json_object_get_at(v, i);
            for (int s = 0; s < indent; s++) fputc(' ', f);
            fprintf(f, "%s:", json_object_get_key_at(v, i));
            if (item && item->type == JSON_STRING)
                fprintf(f, " %s\n", item->str_val);
            else if (item && (item->type == JSON_NUMBER ||
                              item->type == JSON_BOOL))
                p_yaml_emit_scalar(f, item);
            else
                p_yaml_emit(f, item, indent + 2);
        }
    } else if (v && v->type == JSON_ARRAY) {
        fputc('\n', f);
        for (size_t i = 0; i < json_len((json_t *)v); i++) {
            const json_t *item = json_get((json_t *)v, i);
            for (int s = 0; s < indent - 2; s++) fputc(' ', f);
            fputs("- ", f);
            if (item && item->type == JSON_OBJECT) {
                for (size_t k = 0; k < json_object_size(item); k++) {
                    const json_t *iv = json_object_get_at(item, k);
                    if (k > 0) {
                        fputc('\n', f);
                        for (int s = 0; s < indent; s++) fputc(' ', f);
                    }
                    fprintf(f, "%s: ", json_object_get_key_at(item, k));
                    if (iv && iv->type == JSON_STRING)
                        fprintf(f, "%s\n", iv->str_val);
                    else if (iv && iv->type == JSON_NUMBER)
                        p_yaml_emit_scalar(f, iv);
                    else if (iv && iv->type == JSON_BOOL)
                        fprintf(f, "%s\n", iv->bool_val ? "true" : "false");
                    else
                        p_yaml_emit(f, iv, indent + 2);
                }
            } else {
                p_yaml_emit_scalar(f, item);
            }
        }
    } else {
        p_yaml_emit_scalar(f, v);
    }
}

/* PoP: cmd_pack_export @ hermes_cli/plugin_packs.py:cmd_pack_export */
void cmd_pack_export(int enabled_only, const char *name) {
    char home[1024], plugins_dir[1200];
    p_home(home, sizeof home);
    snprintf(plugins_dir, sizeof plugins_dir, "%s/plugins", home);

    json_t *meta = p_read_install_metadata();
    strset_t enabled;
    p_get_name_set("enabled", &enabled);

    DIR *d = opendir(plugins_dir);
    if (!d) {
        fprintf(stderr, "Error: Could not export pack: no plugins directory.\n");
        exit(1);
    }
    char *names[512];
    size_t nnames = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && nnames < 512) {
        if (e->d_name[0] == '.') continue;
        char sub[2400];
        snprintf(sub, sizeof sub, "%s/%s", plugins_dir, e->d_name);
        if (p_is_dir(sub)) names[nnames++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nnames, sizeof(char *), p_cmp_str);

    json_t *entries_arr = json_array();
    json_t *config_obj = json_object();
    json_t *warnings = json_array();

    for (size_t i = 0; i < nnames; i++) {
        const char *pid = names[i];
        if (enabled_only && !strset_contains(&enabled, pid)) continue;
        const json_t *rec = json_obj_get(meta, pid);
        const char *source = rec ? p_json_str(rec, "source", "") : "";
        const char *revision = rec ? p_json_str(rec, "revision", "") : "";

        /* _source_to_repo_subdir: split "#subdir"; collapse GitHub https URL */
        char base[1024] = "", subdir[512] = "";
        {
            const char *hash = source ? strchr(source, '#') : NULL;
            if (hash) {
                size_t bl = (size_t)(hash - source);
                if (bl >= sizeof base) bl = sizeof base - 1;
                memcpy(base, source, bl);
                base[bl] = '\0';
                snprintf(subdir, sizeof subdir, "%s", hash + 1);
                size_t sl = strlen(subdir);
                while (sl && subdir[sl - 1] == '/') subdir[--sl] = '\0';
            } else if (source) {
                snprintf(base, sizeof base, "%s", source);
            }
            const char *gh = "https://github.com/";
            if (strncmp(base, gh, strlen(gh)) == 0) {
                char buf[1024];
                snprintf(buf, sizeof buf, "%s", base + strlen(gh));
                size_t bl2 = strlen(buf);
                if (bl2 >= 4 && strcmp(buf + bl2 - 4, ".git") == 0)
                    buf[bl2 - 4] = '\0';
                char *slash = strchr(buf, '/');
                if (slash && strchr(slash + 1, '/'))
                    *(char *)strchr(slash + 1, '/') = '\0';
                snprintf(base, sizeof base, "%s", buf);
            }
        }
        if (!base[0] || !plugin_pack_is_exact_sha(revision)) {
            char wmsg[1024];
            snprintf(wmsg, sizeof wmsg,
                     "%s: no Git provenance (local-only or pre-metadata "
                     "install) - listed as a comment, not installable from "
                     "this pack.", pid);
            json_append(warnings, json_string(wmsg));
            continue;
        }
        json_t *entry = json_object();
        json_set(entry, "repo", json_string(base));
        json_set(entry, "ref", json_string(revision));
        if (subdir[0]) json_set(entry, "subdir", json_string(subdir));
        json_append(entries_arr, entry);

        /* _sanitized_entry_config: reserved, allow-prefixed and secret-shaped keys stripped */
        json_t *config = p_cfg_load();
        const json_t *plug = config ? json_obj_get(config, "plugins") : NULL;
        const json_t *all_entries =
            (plug && plug->type == JSON_OBJECT)
                ? json_obj_get(plug, "entries") : NULL;
        const json_t *ecfg =
            (all_entries && all_entries->type == JSON_OBJECT)
                ? json_obj_get(all_entries, pid) : NULL;
        if (ecfg && ecfg->type == JSON_OBJECT) {
            json_t *out_cfg = json_object();
            int any = 0;
            for (size_t k = 0; k < json_object_size(ecfg); k++) {
                const char *kk = json_object_get_key_at(ecfg, k);
                if (p_entry_reserved(kk) || p_is_secret_key(kk)) continue;
                json_set(out_cfg, kk,
                         json_copy((json_t *)json_object_get_at(ecfg, k)));
                any = 1;
            }
            if (any)
                json_set(config_obj, pid, out_cfg);
            else
                json_free(out_cfg);
        }
        if (config) json_free(config);
    }
    for (size_t i = 0; i < nnames; i++) free(names[i]);
    strset_free(&enabled);
    json_free(meta);

    for (size_t i = 0; i < json_len(warnings); i++)
        fprintf(stderr, "Warning: %s\n",
                p_json_str(json_get(warnings, i), "", ""));

    printf("name: %s\n", name && *name ? name : "my-hermes-pack");
    printf("description: Exported by `hermes plugins pack export`.\n");
    printf("version: 1.0.0\n");
    fputs("plugins:", stdout);
    if (json_len(entries_arr) == 0)
        printf(" []\n");
    else
        p_yaml_emit(stdout, entries_arr, 2);
    if (json_object_size(config_obj) > 0) {
        fputs("config:", stdout);
        p_yaml_emit(stdout, config_obj, 2);
    }
    json_free(entries_arr);
    json_free(config_obj);
    json_free(warnings);
}
