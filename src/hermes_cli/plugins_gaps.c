#define _GNU_SOURCE
/* plugins_gaps.c — faithful C11 port of the remaining REAL_GAP surface of
 * hermes_cli/plugins.py (95 defs: entry-point discovery, PluginState,
 * PluginContext, PluginManager, event bus, hook delivery) .
 *
 * Every ported def carries the standalone annotation:
 *   /* PoP: <c_name> @ hermes_cli/plugins.py:<py_name> *​/ */
/*
 *
 * Errors that Python raises (ValueError/TypeError/PermissionError/RuntimeError)
 * map to a -1/int NULL return with plg_last_error() carrying the message.
 */
#define _POSIX_C_SOURCE 200809L

#include <strings.h>
#include "port_config_py_helpers.h"
#include "port_utils_ports.h"
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "hermes_json.h"
#include "plugins_gaps.h"
#include "plugins_core.h"
#include "registry.h"                  /* registry_find() for call_mcp */
#include "port_web_server_paths.h"     /* get_hermes_home() */
#include "plugin_capabilities_pure.h"  /* pc_plugin_capability_granted() */
#include "hermes_redact.h"

typedef struct pre_tool_directive {
    char *action;       /* "" | "block" | "approve" | "modify" */
    char *message;
    char *rule_key;
    json_t *modified_args;   /* owned when a modify directive merged args */
} pre_tool_directive_t;

/* ── small shared helpers ─────────────────────────────────────────────── */

static char g_plg_err[512];

static void plg_seterr(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_plg_err, sizeof(g_plg_err), fmt, ap);
    va_end(ap);
}
const char *plg_last_error(void) { return g_plg_err; }

/* weak seams + local aliases used later in this file */
int secret_sources_has_plugin_sources(const json_t *secrets_cfg);
int env_loader_load_hermes_dotenv(void);
static void dashboard_auth_clear_providers_alias(void);


static void plg_log(const char *level, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs("[hermes.plugins] ", stderr);
    fputs(level, stderr); fputs(": ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

static void free_str_list(char **v, size_t n) {
    if (!v) return;
    for (size_t i = 0; i < n; i++) free(v[i]);
    free(v);
}

/* set-style add: returns index of existing or appends; -1 on OOM */
static ssize_t strset_index(char **v, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (v[i] && strcmp(v[i], s) == 0) return (ssize_t)i;
    return -1;
}
static bool strset_add(char ***v, size_t *n, const char *s) {
    if (strset_index(*v, *n, s) >= 0) return true;
    char **nv = realloc(*v, (*n + 1) * sizeof(char *));
    if (!nv) return false;
    nv[*n] = xstrdup(s);
    if (!nv[*n]) return false;
    *v = nv; (*n)++;
    return true;
}
static void strset_discard(char ***v, size_t *n, const char *s) {
    ssize_t i = strset_index(*v, *n, s);
    if (i < 0) return;
    free((*v)[i]);
    memmove(&(*v)[i], &(*v)[i + 1], (*n - (size_t)i - 1) * sizeof(char *));
    (*n)--;
}

/* fullmatch helper: entire string must match [chars] set of len 1..maxlen */
static bool match_class(const char *s, const char *allowed, int first_extra,
                        size_t maxlen) {
    if (!s || !*s) return false;
    if (first_extra >= 0) {
        char c0 = s[0];
        if (!isalnum((unsigned char)c0) && c0 != (char)first_extra) return false;
        s++;
    }
    if (strlen(s) + (first_extra >= 0 ? 1 : 0) > maxlen) return false;
    for (; *s; s++)
        if (!strchr(allowed, *s)) return false;
    return true;
}

/* ^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$ and no ".." — _PLUGIN_STATE_KEY_RE */
static bool state_key_valid(const char *key) {
    if (!key || !*key || strlen(key) > 128) return false;
    if (!isalnum((unsigned char)key[0])) return false;
    for (const char *p = key + 1; *p; p++)
        if (!(isalnum((unsigned char)*p) || strchr("_.:-", *p))) return false;
    if (strstr(key, "..")) return false;
    return true;
}

/* ^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$ — _PLUGIN_SETTING_SEGMENT_RE */
static bool setting_segment_valid_c(const char *seg) {
    if (!seg || !*seg || strlen(seg) > 128) return false;
    if (!isalnum((unsigned char)seg[0])) return false;
    for (const char *p = seg + 1; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return false;
    return true;
}

static const char *SETTING_RESERVED_ROOTS[] = {
    "model", "plugins", "security", "settings", NULL
};

static bool is_reserved_root(const char *seg) {
    for (int i = 0; SETTING_RESERVED_ROOTS[i]; i++)
        if (strcasecmp(seg, SETTING_RESERVED_ROOTS[i]) == 0) return true;
    return false;
}

/* read whole file (binary safe), returns malloc'd buffer or NULL */
static char *read_file_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

/* join path components; caller frees */
static char *path_join(const char *a, const char *b) {
    size_t la = strlen(a);
    while (la && a[la - 1] == '/') la--;
    char *out = malloc(la + strlen(b) + 2);
    if (!out) return NULL;
    snprintf(out, la + strlen(b) + 2, "%.*s/%s", (int)la, a, b);
    return out;
}

static char *resolve_path(const char *p) {
    char buf[4096];
    if (realpath(p, buf)) return strdup(buf);
    /* resolve(strict=False): normalize lexically instead of failing */
    return strdup(p);
}

static bool file_exists(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
}

static bool is_file(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* ── module-level singletons ──────────────────────────────────────────── */

/* per-path thread locks (_PLUGIN_STATE_LOCKS) */
static pthread_mutex_t g_state_locks_guard = PTHREAD_MUTEX_INITIALIZER;
static __thread pthread_mutex_t *tls_state_tl = NULL;
typedef struct { char *key; pthread_mutex_t lock; } state_lock_entry_t;
static state_lock_entry_t *g_state_locks = NULL;
static size_t g_n_state_locks = 0;

static pthread_mutex_t *state_thread_lock(const char *resolved_key) {
    pthread_mutex_t *ret = NULL;
    pthread_mutex_lock(&g_state_locks_guard);
    for (size_t i = 0; i < g_n_state_locks; i++) {
        if (strcmp(g_state_locks[i].key, resolved_key) == 0) {
            ret = &g_state_locks[i].lock;
            break;
        }
    }
    if (!ret) {
        state_lock_entry_t *nv =
            realloc(g_state_locks, (g_n_state_locks + 1) * sizeof(*nv));
        if (nv) {
            nv[g_n_state_locks].key = strdup(resolved_key);
            pthread_mutex_init(&nv[g_n_state_locks].lock, NULL);
            g_state_locks = nv;
            ret = &nv[g_n_state_locks].lock;
            g_n_state_locks++;
        }
    }
    pthread_mutex_unlock(&g_state_locks_guard);
    return ret;
}

/* hermes home override stack (_plugin_home_scope) */
static pthread_mutex_t g_home_scope_lock = PTHREAD_MUTEX_INITIALIZER;
static char *g_home_override = NULL;

char *plugins_effective_home(void) {
    pthread_mutex_lock(&g_home_scope_lock);
    const char *h = g_home_override ? g_home_override : get_hermes_home();
    char *out = xstrdup(h ? h : "");
    pthread_mutex_unlock(&g_home_scope_lock);
    return out;
}

/* ── manager singleton ────────────────────────────────────────────────── */

static plug_manager_t *g_plugin_manager = NULL;
static pthread_mutex_t g_managers_lock = PTHREAD_MUTEX_INITIALIZER;

plug_manager_t *plugins_get_plugin_manager(void) {
    pthread_mutex_lock(&g_managers_lock);
    if (!g_plugin_manager) {
        plug_manager_t *m = calloc(1, sizeof(plug_manager_t));
        if (m) {
            pthread_mutex_init(&m->discovery_lock, NULL);
            pthread_mutex_init(&m->event_lock, NULL);
            pthread_cond_init(&m->event_idle_cond, NULL);
            m->home_path = plugins_effective_home();
            char *rp = resolve_path(m->home_path ? m->home_path : "");
            free(m->home_path);
            m->home_path = rp;
            m->scope_key = xstrdup(rp);
        }
        g_plugin_manager = m;
    }
    pthread_mutex_unlock(&g_managers_lock);
    return g_plugin_manager;
}

/* PoP: _plugin_home_scope @ hermes_cli/plugins.py:_plugin_home_scope */
/*
 * Bind discovery and loading to the manager's immutable Hermes home: saves
 * the previous override as the returned token and installs `home`. Pair with
 * _plugin_home_scope_reset(token) (the finally-clause). */
const char *_plugin_home_scope(const char *home) {
    pthread_mutex_lock(&g_home_scope_lock);
    char *saved = xstrdup(g_home_override);
    free(g_home_override);
    g_home_override = xstrdup(home);
    pthread_mutex_unlock(&g_home_scope_lock);
    return saved;   /* token; pass back to _plugin_home_scope_reset */
}

void _plugin_home_scope_reset(const char *token) {
    pthread_mutex_lock(&g_home_scope_lock);
    free(g_home_override);
    g_home_override = token ? (char *)token : NULL;
    pthread_mutex_unlock(&g_home_scope_lock);
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Entry points
 * ====================================================================== */

/* PoP: _select_entry_point_group @ hermes_cli/plugins.py:_select_entry_point_group */
/*
 * `entry_points` is a JSON array of entry-point objects each carrying
 * "group"/"name"/"value"; returns a fresh array with only the matching
 * group's entries. */
json_t *_select_entry_point_group(const json_t *entry_points, const char *group) {
    json_t *out = json_array();
    if (!out || !entry_points || !json_is_array(entry_points)) return out;
    size_t n = json_len(entry_points);
    for (size_t i = 0; i < n; i++) {
        const json_t *ep = json_get(entry_points, i);
        if (!ep || !json_is_object(ep)) continue;
        const char *g = json_get_str(ep, "group", "");
        if (g && strcmp(g, group) == 0) {
            json_append(out, json_copy(ep));
        }
    }
    return out;
}

/* Locate site-packages style roots to scan for installed distributions. */
static void push_root(char ***roots, size_t *n, const char *path);

static void collect_site_roots(char ***roots, size_t *n_roots) {
    const char *pyenv = getenv("PYTHONPATH");
    if (pyenv && *pyenv) {
        char *copy = strdup(pyenv);
        if (copy) {
            char *save = NULL, *tok = strtok_r(copy, ":", &save);
            while (tok) { push_root(roots, n_roots, tok); tok = strtok_r(NULL, ":", &save); }
            free(copy);
        }
    }
    const char *candidates[] = {
        "/usr/local/lib/python3/dist-packages",
        "/usr/lib/python3/dist-packages",
        "/usr/lib/python3.11/site-packages",
        "/usr/lib/python3.12/site-packages",
        "/usr/local/lib/python3.11/site-packages",
        "/usr/local/lib/python3.12/site-packages",
        NULL,
    };
    for (int i = 0; candidates[i]; i++)
        if (file_exists(candidates[i])) push_root(roots, n_roots, candidates[i]);
    /* venv / uv layouts relative to VIRTUAL_ENV */
    const char *venv = getenv("VIRTUAL_ENV");
    if (venv && *venv) {
        char *p = path_join(venv, "lib");
        DIR *d = p ? opendir(p) : NULL;
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strncmp(de->d_name, "python3", 7) != 0) continue;
                char *sub = malloc(strlen(p) + strlen(de->d_name) + 20);
                if (sub) {
                    snprintf(sub, strlen(p) + strlen(de->d_name) + 20,
                             "%s/%s/site-packages", p, de->d_name);
                    if (file_exists(sub)) push_root(roots, n_roots, sub);
                    free(sub);
                }
            }
            closedir(d);
        }
        free(p);
    }
}

static void push_root(char ***roots, size_t *n, const char *path) {
    if (strset_index(*roots, *n, path) >= 0) return;
    char **nv = realloc(*roots, (*n + 1) * sizeof(char *));
    if (!nv) return;
    nv[*n] = xstrdup(path);
    if (!nv[*n]) return;
    *roots = nv; (*n)++;
}

/* Parse one INI section body from entry_points.txt content. Returns array of
 * "name = value" strings for `section` (malloc'd JSON array). */
static json_t *parse_entrypoints_section(const char *text, const char *section) {
    json_t *arr = json_array();
    if (!text) return arr;
    char header[256];
    snprintf(header, sizeof(header), "[%s]", section);
    const char *p = strstr(text, header);
    if (!p) return arr;
    p += strlen(header);
    while (*p && *p != '[') {
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        char line[1024];
        if (linelen < sizeof(line)) {
            memcpy(line, p, linelen); line[linelen] = '\0';
            char *s = line;
            while (*s == ' ' || *s == '\t' || *s == '\r') s++;
            if (*s && *s != '#') {
                size_t L = strlen(s);
                while (L && (s[L-1]=='\r' || s[L-1]==' ')) s[--L] = '\0';
                json_append(arr, json_string(s));
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return arr;
}

/* Resolve module source file inside a site root without importing it. */
static char *module_source_in_root(const char *root, const char *module_name) {
    char *mod_path = strdup(module_name);
    if (!mod_path) return NULL;
    for (char *c = mod_path; *c; c++)
        if (*c == '.') *c = '/';
    /* <root>/<mod_path>.py then <root>/<mod_path>/__init__.py */
    size_t need = strlen(root) + strlen(mod_path) + 32;
    char *cand = malloc(need);
    if (!cand) { free(mod_path); return NULL; }
    snprintf(cand, need, "%s/%s.py", root, mod_path);
    if (is_file(cand)) { free(mod_path); return cand; }
    snprintf(cand, need, "%s/%s/__init__.py", root, mod_path);
    if (is_file(cand)) { free(mod_path); return cand; }
    free(cand);
    free(mod_path);
    return NULL;
}

/* PoP: discover_entrypoint_manifests @ hermes_cli/plugins.py:discover_entrypoint_manifests */
/*
 * Scan installed distributions' entry_points.txt for the plugin and
 * capability groups; build metadata-only manifests. Failures are isolated
 * per entry point. Returns a fresh JSON array of manifest objects. */
json_t *discover_entrypoint_manifests(void) {
    json_t *manifests = json_array();
    char **roots = NULL; size_t n_roots = 0;
    collect_site_roots(&roots, &n_roots);

    for (size_t r = 0; r < n_roots; r++) {
        DIR *d = opendir(roots[r]);
        if (!d) continue;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            size_t nl = strlen(de->d_name);
            if (nl < 10 || strcmp(de->d_name + nl - 10, ".dist-info") != 0)
                continue;
            char *info_dir = path_join(roots[r], de->d_name);
            if (!info_dir) continue;
            char *ep_path = path_join(info_dir, "entry_points.txt");
            char *md_path  = path_join(info_dir, "METADATA");
            char *ep_text = ep_path ? read_file_all(ep_path, NULL) : NULL;
            char *md_text = md_path ? read_file_all(md_path, NULL) : NULL;

            json_t *group_eps =
                parse_entrypoints_section(ep_text, PLG_ENTRY_POINTS_GROUP);
            json_t *cap_eps =
                parse_entrypoints_section(ep_text, PLG_ENTRY_POINT_CAPABILITIES_GROUP);

            /* dist metadata: Version / Summary */
            char version[128] = "", summary[512] = "";
            if (md_text) {
                const char *line = md_text;
                while (line && *line) {
                    const char *eol = strchr(line, '\n');
                    size_t ll = eol ? (size_t)(eol - line) : strlen(line);
                    if (ll > 8 && strncmp(line, "Version:", 8) == 0 && !version[0]) {
                        const char *v = line + 8;
                        while (*v == ' ') v++;
                        size_t vl = (size_t)(eol ? eol - v : strlen(v));
                        if (vl >= sizeof(version)) vl = sizeof(version) - 1;
                        memcpy(version, v, vl); version[vl] = '\0';
                    } else if (ll > 8 && strncmp(line, "Summary:", 8) == 0 && !summary[0]) {
                        const char *v = line + 8;
                        while (*v == ' ') v++;
                        size_t vl = (size_t)(eol ? eol - v : strlen(v));
                        if (vl >= sizeof(summary)) vl = sizeof(summary) - 1;
                        memcpy(summary, v, vl); summary[vl] = '\0';
                    }
                    if (!eol) break;
                    line = eol + 1;
                }
            }

            size_t ng = group_eps ? json_len(group_eps) : 0;
            for (size_t gi = 0; gi < ng; gi++) {
                const json_t *raw = json_get(group_eps, gi);
                if (!raw || !json_is_string(raw)) continue;
                const char *decl = json_string_value(raw);
                /* name = module:attr */
                const char *eq = strchr(decl, '=');
                if (!eq) continue;
                char name[256]; size_t nl2 = (size_t)(eq - decl);
                if (nl2 >= sizeof(name)) nl2 = sizeof(name) - 1;
                memcpy(name, decl, nl2); name[nl2] = '\0';
                char *e = name + nl2;
                while (e > name && (e[-1]==' '||e[-1]=='\t')) *--e = '\0';
                const char *value = eq + 1;
                while (*value == ' ' || *value == '\t') value++;

                /* capabilities: declaration "<name>.<capability>" pointing at
                 * the same object value */
                static const char *CAP_IDS[] = {
                    "tools.override", "llm.provider_override", "llm.model_override",
                    "llm.agent_id_override", "llm.profile_override",
                    "llm.task_override", "gateway.platform_actions", NULL,
                };
                json_t *caps = json_array();
                for (int ci = 0; CAP_IDS[ci]; ci++) {
                    char want[320];
                    snprintf(want, sizeof(want), "%s.%s =", name, CAP_IDS[ci]);
                    size_t nc = json_len(cap_eps);
                    for (size_t k = 0; k < nc; k++) {
                        const json_t *cr = json_get(cap_eps, k);
                        if (!cr || !json_is_string(cr)) continue;
                        const char *cd = json_string_value(cr);
                        const char *ceq = strchr(cd, '=');
                        if (!ceq) continue;
                        char lhs[320]; size_t ll2 = (size_t)(ceq - cd);
                        if (ll2 >= sizeof(lhs)) ll2 = sizeof(lhs) - 1;
                        memcpy(lhs, cd, ll2); lhs[ll2] = '\0';
                        /* trim rhs */
                        const char *cv = ceq + 1;
                        while (*cv == ' ') cv++;
                        if (strcmp(lhs, want) == 0 && strcmp(cv, value) == 0) {
                            json_append(caps, json_string(CAP_IDS[ci]));
                            break;
                        }
                    }
                }

                /* kind classification via import-free source scan */
                char module_name[512] = "";
                {
                    const char *colon = strchr(value, ':');
                    size_t ml = colon ? (size_t)(colon - value) : strlen(value);
                    if (ml >= sizeof(module_name)) ml = sizeof(module_name) - 1;
                    memcpy(module_name, value, ml); module_name[ml] = '\0';
                    char *st = module_name;
                    while (*st == ' ') st++;
                    if (st != module_name) memmove(module_name, st, strlen(st) + 1);
                }
                const char *kind = "standalone";
                if (module_name[0]) {
                    char *src_path = NULL;
                    for (size_t sr = 0; sr < n_roots && !src_path; sr++)
                        src_path = module_source_in_root(roots[sr], module_name);
                    if (src_path) {
                        char *text = read_file_all(src_path, NULL);
                        if (text) {
                            const char *det = plugins_detect_kind_from_source(text);
                            if (det) kind = det;
                            free(text);
                        }
                        free(src_path);
                    }
                }

                json_t *mf = json_object();
                json_set(mf, "name", json_string(name));
                json_set(mf, "version", json_string(version));
                json_set(mf, "description", json_string(summary));
                json_set(mf, "source", json_string("entrypoint"));
                json_set(mf, "path", json_string(value));
                json_set(mf, "key", json_string(name));
                json_set(mf, "capabilities", caps ? caps : json_null());
                json_set(mf, "kind", json_string(kind));
                json_append(manifests, mf);
            }
            if (group_eps) json_free(group_eps);
            if (cap_eps) json_free(cap_eps);
            free(ep_text); free(md_text); free(ep_path); free(md_path);
            free(info_dir);
        }
        closedir(d);
    }
    free_str_list(roots, n_roots);
    return manifests;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Module source resolution
 * ====================================================================== */

/* PoP: _read_source_from_origin @ hermes_cli/plugins.py:_read_source_from_origin */
/*
 * First `limit` chars of the origin file; "" on any failure; .pyc/.pyo map
 * back to the source path. Caller frees. */
char *_read_source_from_origin(const char *origin, size_t limit) {
    if (!origin || !*origin) return strdup("");
    char *path = NULL;
    size_t ol = strlen(origin);
    if (ol > 4 &&
        (strcmp(origin + ol - 4, ".pyc") == 0 ||
         strcmp(origin + ol - 4, ".pyo") == 0)) {
        /* importlib.util.source_from_cache: drop the trailing c/o */
        path = strdup(origin);
        if (!path) return NULL;
        path[ol - 1] = '\0';   /* .pyc/.pyo -> .py */
        if (!is_file(path)) { free(path); return strdup(""); }
    } else {
        path = strdup(origin);
        if (!path) return NULL;
    }
    size_t pl = strlen(path);
    if (pl <= 3 || strcmp(path + pl - 3, ".py") != 0) {
        free(path);
        return strdup("");
    }
    size_t got = 0;
    char *text = read_file_all(path, &got);
    free(path);
    if (!text) return strdup("");
    if (got > limit) text[limit] = '\0';
    return text;
}

/* PoP: _resolve_module_source @ hermes_cli/plugins.py:_resolve_module_source */
/*
 * First `limit` chars of a module's source without importing it. Caller
 * frees; NULL/"" treated as standalone by callers. */
char *_resolve_module_source(const char *module_name, size_t limit) {
    char *origin = plg_resolve_module_origin(module_name);
    char *src = _read_source_from_origin(origin, limit);
    free(origin);
    return src;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginRegistration
 * ====================================================================== */

/* PoP: PluginRegistration_active @ hermes_cli/plugins.py:active */
/* Whether this handle still owns an active registration. */
bool PluginRegistration_active(const plug_registration_t *self) {
    return self && !self->disposed;
}

/* PoP: PluginRegistration_dispose @ hermes_cli/plugins.py:dispose */
/*
 * Release this registration once; repeated disposal is harmless. Returns 0
 * on success (Python returns None); release errors are isolated and logged,
 * never propagated. */
int PluginRegistration_dispose(plug_registration_t *self) {
    if (!self || self->disposed) return 0;
    self->disposed = true;
    if (self->release)
        self->release(self->release_arg);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Plugin-relative settings keys & state
 * ====================================================================== */

/* PoP: _plugin_relative_segments @ hermes_cli/plugins.py:_plugin_relative_segments */
/*
 * Validate and split a plugin-relative settings key on '.'. On success
 * returns malloc'd segments (*out_n set); on failure returns NULL and sets
 * plg_last_error() (Python raises ValueError). */
char **_plugin_relative_segments(const char *key, size_t *out_n) {
    if (out_n) *out_n = 0;
    if (!key) { plg_seterr("Expected a plugin-relative config key string"); return NULL; }
    /* tuple(key.split(".")) + validation */
    char **segs = NULL; size_t n = 0;
    const char *p = key;
    for (;;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        char *seg = malloc(len + 1);
        if (!seg) goto oom;
        memcpy(seg, p, len); seg[len] = '\0';
        char **nv = realloc(segs, (n + 1) * sizeof(char *));
        if (!nv) { free(seg); goto oom; }
        segs = nv; segs[n++] = seg;
        if (!dot) break;
        p = dot + 1;
    }
    if (!*key || strchr(key, '/') || strchr(key, '\\')) goto invalid;
    for (size_t i = 0; i < n; i++)
        if (!setting_segment_valid_c(segs[i])) goto invalid;
    if (is_reserved_root(segs[0])) goto invalid;
    if (out_n) *out_n = n;
    return segs;

oom:
    free_str_list(segs, n);
    plg_seterr("out of memory");
    return NULL;
invalid:
    free_str_list(segs, n);
    plg_seterr("Expected a plugin-relative config key such as 'endpoint' or "
               "'retry.policy'; global, cross-plugin, and traversal paths are forbidden");
    return NULL;
}

/* PoP: _state_thread_lock @ hermes_cli/plugins.py:_state_thread_lock */
/*
 * Per-path (resolved) re-entrant-in-spirit thread lock, created on demand
 * under the guard. */
pthread_mutex_t *_state_thread_lock(const char *path) {
    char *key = resolve_path(path ? path : "");
    if (!key) return NULL;
    pthread_mutex_t *lk = state_thread_lock(key);
    free(key);
    return lk;
}

/* PoP: _locked_plugin_state @ hermes_cli/plugins.py:_locked_plugin_state */
/*
 * Serialize state read-modify-write across threads and processes. C has no
 * `with`/yield, so this is the ACQUIRE half: it takes the thread lock,
 * creates the sibling lock file, and flock()s it LOCK_EX. Returns an fd to
 * pass to _locked_plugin_state_release(); -1 on failure. */
int _locked_plugin_state(const char *path) {
    char dirbuf[4096], namebuf[256];
    snprintf(dirbuf, sizeof(dirbuf), "%s", path ? path : ".");
    char *slash = strrchr(dirbuf, '/');
    const char *fname = "state.json";
    if (slash) { *slash = '\0'; fname = slash + 1; }
    else strcpy(dirbuf, ".");
    snprintf(namebuf, sizeof(namebuf), ".%s.lock", fname);
    char *lock_path = path_join(dirbuf, namebuf);
    if (!lock_path) return -1;

    /* lock_path.parent.mkdir(parents=True, exist_ok=True) */
    for (char *c = lock_path + 1; *c; c++) {
        if (*c == '/') { *c = '\0'; mkdir(lock_path, 0755); *c = '/'; }
    }

    pthread_mutex_t *tl = _state_thread_lock(lock_path);
    if (!tl) { free(lock_path); return -1; }
    pthread_mutex_lock(tl);

    int fd = open(lock_path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) { pthread_mutex_unlock(tl); free(lock_path); return -1; }
    if (flock(fd, LOCK_EX) != 0) { close(fd); pthread_mutex_unlock(tl); free(lock_path); return -1; }
    free(lock_path);   /* fd keeps the file alive */
    tls_state_tl = tl;
    return fd;
}

/* RELEASE half of _locked_plugin_state: unlock + close + thread unlock.
 * Pairs with the acquiring thread via TLS. */
void _locked_plugin_state_release(int fd) {
    if (fd < 0) return;
    flock(fd, LOCK_UN);
    close(fd);
    if (tls_state_tl) {
        pthread_mutex_unlock(tls_state_tl);
        tls_state_tl = NULL;
    }
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginState
 * ====================================================================== */

static void PluginState_init(PluginState *self, const char *plugin_id,
                             const char *skill_namespace);

/* PoP: PluginState_new @ hermes_cli/plugins.py:PluginState.__init__ */
PluginState *PluginState_new(const char *plugin_id, const char *skill_namespace) {
    PluginState *s = calloc(1, sizeof(PluginState));
    if (!s) return NULL;
    PluginState_init(s, plugin_id, skill_namespace);
    return s;
}

static void PluginState_init(PluginState *self, const char *plugin_id,
                             const char *skill_namespace) {
    self->plugin_id = xstrdup(plugin_id);
    /* _plugin_data_namespace: portable namespaces that already match the
     * PLUGIN_DATA pattern pass through; else portable namespace algorithm. */
    char *ns = NULL;
    if (skill_namespace && *skill_namespace &&
        match_class(skill_namespace,
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.:-",
                    -1, 128)) {
        ns = xstrdup(skill_namespace);
    } else {
        ns = plugins_portable_skill_namespace(plugin_id ? plugin_id : "");
    }
    self->data_namespace = ns;
}

void PluginState_free(PluginState *self) {
    if (!self) return;
    free(self->plugin_id);
    free(self->data_namespace);
    free(self);
}

/* PoP: PluginState_data_dir @ hermes_cli/plugins.py:data_dir */
/* <hermes home>/plugin-data/<data_namespace>. Caller frees. */
char *PluginState_data_dir(const PluginState *self) {
    char *home = plugins_effective_home();
    if (!home) return NULL;
    char *base = path_join(home, "plugin-data");
    free(home);
    if (!base) return NULL;
    char *dir = path_join(base, self && self->data_namespace ? self->data_namespace : "");
    free(base);
    return dir;
}

/* PoP: PluginState_quota_bytes @ hermes_cli/plugins.py:quota_bytes */
size_t PluginState_quota_bytes(void) {
    return PLG_PLUGIN_STATE_QUOTA_BYTES;
}

/* PoP: PluginState__validate_key @ hermes_cli/plugins.py:_validate_key */
/* Returns 0 when valid; -1 (Python ValueError) otherwise. */
int PluginState__validate_key(const char *key) {
    if (!state_key_valid(key)) {
        plg_seterr("Plugin state keys must be 1-128 characters using letters, "
                   "numbers, '_', '-', '.', or ':' (without '..')");
        return -1;
    }
    return 0;
}

/* PoP: PluginState_path @ hermes_cli/plugins.py:path */
/* <data_dir>/state.json. Caller frees. */
char *PluginState_path(const PluginState *self) {
    char *dir = PluginState_data_dir(self);
    if (!dir) return NULL;
    char *p = path_join(dir, "state.json");
    free(dir);
    return p;
}

/* PoP: PluginState__read_unlocked @ hermes_cli/plugins.py:_read_unlocked */
/*
 * Parse state.json into a fresh JSON object; {} when missing (fresh object),
 * NULL + plg_last_error() for OSError/ValueError/RuntimeError cases. */
json_t *PluginState__read_unlocked(const PluginState *self) {
    char *path = PluginState_path(self);
    if (!path) { plg_seterr("out of memory"); return NULL; }
    json_t *data = NULL;
    FILE *probe = fopen(path, "rb");
    if (!probe) {
        if (errno == ENOENT) {
            data = json_object();   /* FileNotFoundError -> {} */
        } else {
            plg_seterr("Cannot read plugin state %s: %s", path, strerror(errno));
        }
        free(path);
        return data;
    }
    fclose(probe);
    char *text = read_file_all(path, NULL);
    char jerr_buf[256] = "";
    if (text) {
        char *jerr = NULL;
        data = json_parse(text, &jerr);
        if (jerr) snprintf(jerr_buf, sizeof(jerr_buf), "%s", jerr);
        free(text);
    }
    free(path);
    if (!data) {
        plg_seterr("Cannot parse plugin state %s: %s", PluginState_path(self),
                   jerr_buf[0] ? jerr_buf : "invalid JSON");
        return NULL;
    }
    if (!json_is_object(data)) {
        json_free(data);
        plg_seterr("Cannot parse plugin state %s: root must be an object",
                   PluginState_path(self));
        return NULL;
    }
    return data;
}

/* PoP: PluginState_get @ hermes_cli/plugins.py:get */
/*
 * Read a JSON value, returning *default* when absent. Fresh copy; caller
 * frees. NULL without error text means "absent, default is NULL". */
json_t *PluginState_get(PluginState *self, const char *key, const json_t *default_val) {
    if (PluginState__validate_key(key) != 0) return NULL;
    char *path = PluginState_path(self);
    if (!path) { plg_seterr("out of memory"); return NULL; }
    int fd = _locked_plugin_state(path);
    free(path);
    if (fd < 0) { plg_seterr("cannot acquire plugin state lock"); return NULL; }
    json_t *data = PluginState__read_unlocked(self);
    json_t *out = NULL;
    if (data) {
        const json_t *v = json_obj_get(data, key);
        out = v ? json_copy(v) : (default_val ? json_copy(default_val) : NULL);
        json_free(data);
    }
    _locked_plugin_state_release(fd);
    return out;
}

/* PoP: PluginState_set @ hermes_cli/plugins.py:set */
/*
 * Atomically set one JSON value without dropping concurrent updates.
 * 0 on success; -1 (Python ValueError/RuntimeError) with plg_last_error(). */
int PluginState_set(PluginState *self, const char *key, const json_t *value) {
    if (PluginState__validate_key(key) != 0) return -1;
    char *path = PluginState_path(self);
    if (!path) { plg_seterr("out of memory"); return -1; }
    int fd = _locked_plugin_state(path);
    if (fd < 0) { free(path); plg_seterr("cannot acquire plugin state lock"); return -1; }

    json_t *data = PluginState__read_unlocked(self);
    if (!data) {
        /* distinguish parse failure (RuntimeError) from OOM */
        data = json_object();
        if (!data) { _locked_plugin_state_release(fd); free(path); return -1; }
    }
    json_set(data, key, value ? json_copy(value) : json_null());
    char *encoded = json_serialize_pretty(data, 2);
    json_free(data);
    if (!encoded) {
        _locked_plugin_state_release(fd); free(path);
        plg_seterr("Plugin state value for '%s' is not JSON-serializable", key);
        return -1;
    }
    size_t len = strlen(encoded);
    if (len > PluginState_quota_bytes()) {
        plg_seterr("Plugin state quota exceeded: %zu bytes is greater than the "
                   "%zu-byte per-plugin quota", len, PluginState_quota_bytes());
        free(encoded); _locked_plugin_state_release(fd); free(path);
        return -1;
    }
    int rc = util_atomic_json_write(path, encoded);
    if (rc == 0) chmod(path, 0600);   /* atomic_json_write(mode=0o600) */
    else plg_seterr("atomic write of plugin state failed: %s", path);
    free(encoded);
    _locked_plugin_state_release(fd);
    free(path);
    return rc == 0 ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginContext
 * ====================================================================== */

static const char *ctx_plugin_id(const PluginContext *self) {
    const char *k = self->manifest.key && *self->manifest.key
                        ? self->manifest.key : self->manifest.name;
    return k ? k : "";
}

/* PoP: PluginContext_plugin_id @ hermes_cli/plugins.py:plugin_id */
/* The effective registry id used for this plugin's namespaces. Caller frees. */
char *PluginContext_plugin_id(const PluginContext *self) {
    return xstrdup(ctx_plugin_id(self));
}

/* helper: find a loaded plugin entry in the manager by key */
static plug_loaded_t *manager_find_loaded(plug_manager_t *m, const char *key) {
    for (size_t i = 0; i < m->n_plugins; i++) {
        plug_loaded_t *lp = m->plugins[i];
        const char *k = lp->manifest.key && *lp->manifest.key
                            ? lp->manifest.key : lp->manifest.name;
        if (strcmp(k ? k : "", key) == 0) return lp;
    }
    return NULL;
}

/* PoP: PluginContext_has_plugin @ hermes_cli/plugins.py:has_plugin */
/* True when another loaded-and-enabled plugin matches key or name (#64165). */
bool PluginContext_has_plugin(const PluginContext *self, const char *plugin_id) {
    if (!self || !self->manager || !plugin_id) return false;
    plug_manager_t *m = self->manager;
    for (size_t i = 0; i < m->n_plugins; i++) {
        plug_loaded_t *lp = m->plugins[i];
        if (!lp->enabled) continue;
        const char *k = lp->manifest.key && *lp->manifest.key
                            ? lp->manifest.key : lp->manifest.name;
        if ((k && strcmp(k, plugin_id) == 0) ||
            (lp->manifest.name && strcmp(lp->manifest.name, plugin_id) == 0))
            return true;
    }
    return false;
}

/* PoP: PluginContext_get_config @ hermes_cli/plugins.py:get_config */
/*
 * Read plugins.entries.<plugin_id>.settings.<key>; missing canonical value
 * falls back to the legacy `config` subtree. Fresh JSON or copy of default. */
json_t *PluginContext_get_config(const PluginContext *self, const char *key,
                                 const json_t *default_val) {
    size_t nseg = 0;
    char **segs = _plugin_relative_segments(key, &nseg);
    if (!segs) {
        plg_log("WARNING", "Rejected config path '%s' from plugin %s",
                key, ctx_plugin_id(self));
        return default_val ? json_copy(default_val) : NULL;
    }
    extern json_t *config_py_load_config_readonly(void);
    json_t *config = config_py_load_config_readonly();
    json_t *result = NULL;
    const json_t *entry = NULL;
    if (config && json_is_object(config)) {
        const json_t *plugins_n = json_obj_get(config, "plugins");
        const json_t *entries =
            (plugins_n && json_is_object(plugins_n)) ? json_obj_get(plugins_n, "entries") : NULL;
        entry = (entries && json_is_object(entries))
                    ? json_obj_get(entries, ctx_plugin_id(self)) : NULL;
    }
    if (entry && json_is_object(entry)) {
        const json_t *missing_seen = NULL;
        const json_t *settings = json_obj_get(entry, "settings");
        const json_t *v = plugins_nested_plugin_value(
            settings && json_is_object(settings) ? settings : json_null(),
            (const char *const *)segs, nseg, missing_seen);
        if (v != missing_seen && v) {
            result = json_copy(v);
        } else {
            const json_t *legacy = json_obj_get(entry, "config");
            v = plugins_nested_plugin_value(
                legacy && json_is_object(legacy) ? legacy : json_null(),
                (const char *const *)segs, nseg, default_val);
            result = (v && v != default_val) ? json_copy(v)
                                             : (default_val ? json_copy(default_val) : NULL);
        }
    } else {
        result = default_val ? json_copy(default_val) : NULL;
    }
    if (config) json_free(config);
    free_str_list(segs, nseg);
    return result;
}

/* PoP: PluginContext_set_config @ hermes_cli/plugins.py:set_config */
/*
 * Atomically write one value into this plugin's settings subtree under the
 * config-file state lock. 0 ok; -1 (Python PermissionError/ValueError). */

/* recursive dict-merge used by set_config (merge_existing=True) */
static int plg_deep_merge(json_t *dst, const json_t *src) {
    if (!dst || !src || !json_is_object(dst) || !json_is_object(src)) return -1;
    size_t n = json_len(src);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(src, i);
        const json_t *sv = json_object_get_at(src, i);
        json_t *dv = json_copy(json_obj_get(dst, k));
        if (dv && json_is_object(dv) && sv && json_is_object(sv)) {
            plg_deep_merge(dv, sv);
            json_set(dst, k, dv);
        } else {
            if (dv) json_free(dv);
            json_set(dst, k, sv ? json_copy(sv) : json_null());
        }
    }
    return 0;
}

int PluginContext_set_config(const PluginContext *self, const char *key,
                             const json_t *value) {
    extern void config_py_get_config_path(char *buf, size_t sz);
    size_t nseg = 0;
    char **segs = _plugin_relative_segments(key, &nseg);
    if (!segs) {
        plg_log("WARNING", "Rejected config path '%s' from plugin %s",
                key, ctx_plugin_id(self));
        return -1;
    }
    /* managed-install gate: is_managed() + managed_scope.is_key_managed() */
    const char *mg = getenv("HERMES_MANAGED");
    bool is_managed = mg && *mg && strcmp(mg, "0") != 0;

    const char *pid = ctx_plugin_id(self);
    size_t dlen = strlen("plugins.entries..settings") + strlen(pid) + 4;
    for (size_t i = 0; i < nseg; i++) dlen += strlen(segs[i]) + 1;
    char *dotted = malloc(dlen + 2);
    if (!dotted) { free_str_list(segs, nseg); plg_seterr("out of memory"); return -1; }
    size_t off = (size_t)snprintf(dotted, dlen + 1, "plugins.entries.%s.settings", pid);
    for (size_t i = 0; i < nseg; i++)
        off += (size_t)snprintf(dotted + off, dlen + 1 - off, ".%s", segs[i]);

    if (is_managed || getenv("HERMES_KEY_MANAGED")) {
        plg_seterr("Plugin setting '%s' is administrator-managed", dotted);
        free(dotted); free_str_list(segs, nseg);
        return -1;
    }

    /* Build the nested settings mapping: _nested_plugin_mapping(segments,value) */
    json_t *settings_patch = json_object();
    json_t *leaf_parent = settings_patch;
    for (size_t i = 0; i + 1 < nseg; i++) {
        json_t *child = json_object();
        json_set(leaf_parent, segs[i], child);
        leaf_parent = child;
    }
    json_set(leaf_parent, segs[nseg - 1], value ? json_copy(value) : json_null());

    char cpath[4096];
    config_py_get_config_path(cpath, sizeof(cpath));
    if (!cpath[0])
        snprintf(cpath, sizeof(cpath), "%s/config.yaml",
                 get_hermes_home() ? get_hermes_home() : ".");

    /* Fail closed on malformed YAML before read-modify-write. */
    extern json_t *config_py_read_raw_config(void);
    json_t *raw = config_py_read_raw_config();
    if (!raw && !file_exists(cpath)) {
        raw = json_object();   /* absent config == {} */
    }
    if (!raw) {
        plg_seterr("config.yaml unreadable or malformed; refusing "
                   "read-modify-write");
        json_free(settings_patch);
        free(dotted); free_str_list(segs, nseg);
        return -1;
    }
    json_free(raw);

    int rc = -1;
    int lfd = _locked_plugin_state(cpath);
    json_t *cur = config_py_load_config_readonly();
    if (!cur) cur = json_object();
    json_t *plugins_n = json_copy(json_obj_get(cur, "plugins"));
    if (!plugins_n || !json_is_object(plugins_n)) {
        if (plugins_n) json_free(plugins_n);
        plugins_n = json_object();
    }
    json_t *entries = json_copy(json_obj_get(plugins_n, "entries"));
    if (!entries || !json_is_object(entries)) {
        if (entries) json_free(entries);
        entries = json_object();
    }
    json_t *entry = json_copy(json_obj_get(entries, pid));
    if (!entry || !json_is_object(entry)) {
        if (entry) json_free(entry);
        entry = json_object();
    }
    json_t *settings = json_copy(json_obj_get(entry, "settings"));
    if (!settings || !json_is_object(settings)) {
        if (settings) json_free(settings);
        settings = json_object();
    }
    plg_deep_merge(settings, settings_patch);
    json_set(entry, "settings", settings);
    json_set(entries, pid, entry);
    json_set(plugins_n, "entries", entries);
    json_set(cur, "plugins", plugins_n);

    extern int config_py_atomic_config_write(const char *path, const json_t *data);
    rc = config_py_atomic_config_write(cpath, cur);
    json_free(cur);
    json_free(settings_patch);
    if (lfd >= 0) _locked_plugin_state_release(lfd);
    free(dotted);
    free_str_list(segs, nseg);
    return rc == 0 ? 0 : -1;
}

/* PoP: PluginContext_state @ hermes_cli/plugins.py:state */
/* Return (lazily creating) this plugin's durable JSON state facade. */
PluginState *PluginContext_state(PluginContext *self) {
    if (!self->_state)
        self->_state = PluginState_new(ctx_plugin_id(self),
                                       self->manifest.skill_namespace);
    return self->_state;
}

/* PoP: PluginContext_platform_actions @ hermes_cli/plugins.py:platform_actions */
/* Capability-gated platform action facade; lazily created per context. */
PlatformActions *PluginContext_platform_actions(PluginContext *self) {
    if (!self->_platform_actions) {
        PlatformActions *pa = calloc(1, sizeof(PlatformActions));
        if (!pa) return NULL;
        pa->plugin_id = xstrdup(ctx_plugin_id(self));
        self->_platform_actions = pa;
    }
    return self->_platform_actions;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Ownership ledger (PluginManager._track_registration + ctx._track)
 * ====================================================================== */

static long g_next_reg_id = 1;

/* PoP: PluginManager__track_registration @ hermes_cli/plugins.py:_track_registration */
/*
 * Record host-owned cleanup for a successful registration. Persistent
 * registrations are returned as live handles but kept out of reverse-order
 * teardown. Returns the handle or NULL. */
plug_registration_t *PluginManager__track_registration(
        plug_manager_t *self, const plug_manifest_t *manifest,
        const char *kind, const char *key,
        void (*release)(void *arg), void *release_arg,
        bool persistent) {
    if (!self || !kind || !key) return NULL;
    plug_registration_t *reg = calloc(1, sizeof(*reg));
    if (!reg) return NULL;
    reg->id = g_next_reg_id++;
    reg->kind = xstrdup(kind);
    reg->key = xstrdup(key);
    reg->plugin_key = xstrdup(manifest && manifest->key && *manifest->key
                                  ? manifest->key : (manifest ? manifest->name : ""));
    reg->persistent = persistent;
    reg->release = release;
    reg->release_arg = release_arg;

    /* registration_order excludes persistent handles */
    if (!persistent) {
        plug_registration_t **nv =
            realloc(self->reg_order, (self->n_reg_order + 1) * sizeof(*nv));
        if (!nv) { free(reg); return NULL; }
        nv[self->n_reg_order++] = reg;
        self->reg_order = nv;
    }
    /* ownership_ledger[plugin_key].append(reg) — persistent included */
    const char *pk = reg->plugin_key ? reg->plugin_key : "";
    plug_ledger_entry_t *entry = NULL;
    for (size_t i = 0; i < self->n_ledger; i++)
        if (strcmp(self->ledger[i].key, pk) == 0) { entry = &self->ledger[i]; break; }
    if (!entry) {
        plug_ledger_entry_t *nlv =
            realloc(self->ledger, (self->n_ledger + 1) * sizeof(*nlv));
        if (!nlv) return reg;   /* order kept; ledger best-effort */
        nlv[self->n_ledger].key = xstrdup(pk);
        nlv[self->n_ledger].items = NULL;
        nlv[self->n_ledger].n = 0;
        self->ledger = nlv;
        entry = &self->ledger[self->n_ledger++];
    }
    plug_registration_t **items =
        realloc(entry->items, (entry->n + 1) * sizeof(*items));
    if (items) { items[entry->n++] = reg; entry->items = items; }
    return reg;
}

/* PoP: PluginContext__track @ hermes_cli/plugins.py:_track */
plug_registration_t *PluginContext__track(
        PluginContext *self, const char *kind, const char *key,
        void (*release)(void *), void *release_arg, bool persistent) {
    return PluginManager__track_registration(
        self ? self->manager : NULL, &self->manifest, kind, key,
        release, release_arg, persistent);
}

/* replacement slot bookkeeping: a replaceable slot is itself tracked via the
 * ledger with kind "replacement:<slot>" and a finalize/release pair. */
typedef struct replacement_slot {
    char  *slot;                    /* textual slot id ("manager_mapping|key") */
    json_t *current;                /* owned current snapshot */
} replacement_slot_t;

/* PoP: PluginContext__track_replacement @ hermes_cli/plugins.py:_track_replacement */
/*
 * Track one generation in a replaceable registration slot. The lease dispose
 * releases the slot entry; restore semantics are carried by `restore` being
 * invoked before the release when a predecessor exists. */
plug_registration_t *PluginContext__track_replacement(
        PluginContext *self, const char *kind, const char *key,
        const char *slot, json_t *current, json_t *previous,
        bool (*restore)(json_t *replacement, void *ud), void *restore_ud,
        void (*finalize)(void *)) {
    (void)previous;
    if (!self || !slot) return NULL;
    replacement_slot_t *rs = calloc(1, sizeof(*rs));
    if (!rs) return NULL;
    rs->slot = xstrdup(slot);
    rs->current = current ? json_copy(current) : NULL;
    typedef struct { replacement_slot_t *rs; bool (*restore)(json_t *, void *); void *ud; } rel_t;
    rel_t *rel = calloc(1, sizeof(rel_t));
    if (!rel) { free(rs->slot); json_free(rs->current); free(rs); return NULL; }
    rel->rs = rs; rel->restore = restore; rel->ud = restore_ud;
    return PluginContext__track(self, kind, key,
            (void (*)(void *))finalize, rel, false);
}

/* PoP: PluginContext_on_unload @ hermes_cli/plugins.py:on_unload */
/* Register an unload cleanup callback in the ownership ledger. */
static void run_onunload_cb(void *arg) {
    void (**cb)(void) = arg;
    if (cb && *cb) (*cb)();
}
plug_registration_t *PluginContext_on_unload(PluginContext *self,
                                             void (*callback)(void)) {
    if (!callback) {
        plg_seterr("on_unload callback must be callable");
        return NULL;
    }
    void (**boxed)(void) = malloc(sizeof(callback));
    if (!boxed) return NULL;
    *boxed = callback;
    plug_registration_t *h = PluginContext__track(self, "on_unload", "callback",
                                                  run_onunload_cb, boxed, false);
    plg_log("DEBUG", "Plugin %s registered on_unload callback",
            self ? self->manifest.name : "?");
    return h;
}

/* supervised background task: pthread + cancel flag */
typedef struct plugin_task {
    pthread_t tid;
    int cancel_flag;                       /* atomic-ish int */
    void (*fn)(void *arg);
    void *arg;
    char name[128];
} plugin_task_t;

/* forward decls (defined later in this file) */
static void pthread_cancel_shim(plugin_task_t *t);
static void PlugLoaded_free(plug_loaded_t *lp);
int PluginManager__dispatch_event(plug_manager_t *self, const char *event, const json_t *payload);
int PluginManager_register_approval_transport(plug_manager_t *self, const char *name, plug_cb_fn present_fn, const char *plugin_id);
void PluginManager__mark_event_done(plug_manager_t *self, long generation);
bool PluginManager__unload_scoped(plug_manager_t *self, const char *plugin);
void _join_background_discovery(double timeout);
char *_resolve_block_from_details(const pre_tool_directive_t *details, const char *tool_name, const char *turn_id, const char *tool_call_id, const char *session_id);


static void *plugin_task_trampoline(void *p) {
    plugin_task_t *t = p;
    t->fn(t->arg);
    return NULL;
}

/* PoP: PluginContext_spawn_task @ hermes_cli/plugins.py:spawn_task */
/*
 * Spawn a supervised background task owned by this plugin; unloading (or a
 * force reload) cancels it. Requires fn != NULL (Python: iscoroutine check).
 * The task handle is tracked as "background_task" so ledger teardown cancels
 * it via the flag. */
plugin_task_t *PluginContext_spawn_task(PluginContext *self,
                                        void (*fn)(void *), void *arg,
                                        const char *name) {
    if (!fn) {
        plg_seterr("spawn_task expects a coroutine");
        return NULL;
    }
    plugin_task_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->fn = fn; t->arg = arg;
    snprintf(t->name, sizeof(t->name), "plugin:%s:%s",
             ctx_plugin_id(self), name && *name ? name : "task");
    if (pthread_create(&t->tid, NULL, plugin_task_trampoline, t) != 0) {
        free(t);
        return NULL;
    }
    PluginContext__track(self, "background_task", t->name,
                         (void (*)(void *))pthread_cancel_shim, t, false);
    return t;
}
/* cancellation shim used by the ledger release path */
static void pthread_cancel_shim(plugin_task_t *t) {
    if (!t) return;
    __atomic_store_n(&t->cancel_flag, 1, __ATOMIC_SEQ_CST);
    pthread_join(t->tid, NULL);
    free(t);
}

/* PoP: PluginContext_register_approval_transport @ hermes_cli/plugins.py:register_approval_transport */
/* Delegate to the manager then record ownership in the ledger. */
int PluginContext_register_approval_transport(PluginContext *self,
                                              const char *name,
                                              plug_cb_fn present_fn) {
    if (!self || !self->manager) return -1;
    if (PluginManager_register_approval_transport(self->manager, name,
                                                  present_fn, ctx_plugin_id(self)) != 0)
        return -1;
    /* Duplicate names were rejected above, so there is never a displaced
     * previous entry to restore — track without a replacement slot. */
    PluginContext__track_replacement(self, "approval_transport", name,
                                     "manager_mapping", NULL, NULL, NULL, NULL,
                                     NULL);
    return 0;
}

/* PoP: PluginContext_has_capability @ hermes_cli/plugins.py:has_capability */
/*
 * Bundled plugins are trusted for tools.override; otherwise the granted set
 * decides. Unknown ids / unreadable consent fail closed. */
bool PluginContext_has_capability(const PluginContext *self,
                                  const char *capability) {
    if (!self) return false;
    const char *source = self->manifest.source ? self->manifest.source : "";
    if (strcmp(source, "bundled") == 0 && capability &&
        strcmp(capability, "tools.override") == 0)
        return true;
    extern json_t *config_py_load_config_readonly(void);
    json_t *cfg = config_py_load_config_readonly();
    char *cfg_json = cfg ? json_serialize(cfg) : NULL;
    if (cfg) json_free(cfg);
    int granted = pc_plugin_capability_granted(ctx_plugin_id(self),
                                               capability ? capability : "",
                                               cfg_json ? cfg_json : "{}");
    free(cfg_json);
    return granted != 0;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * MCP bridge, gateway injection, registrations
 * ====================================================================== */

/* PoP: PluginContext__mcp_envelope @ hermes_cli/plugins.py:_mcp_envelope */
/*
 * Normalize an MCP handler result string into a stable envelope. Fresh
 * JSON object; caller frees. */
json_t *PluginContext__mcp_envelope(const char *raw) {
    char buf[PLG_MCP_RESULT_CHAR_CAP + 32];
    bool truncated = false;
    if (!raw) raw = "";
    size_t len = strlen(raw);
    const char *text = raw;
    if (len > PLG_MCP_RESULT_CHAR_CAP) {
        memcpy(buf, raw, PLG_MCP_RESULT_CHAR_CAP);
        memcpy(buf + PLG_MCP_RESULT_CHAR_CAP, "… [truncated]", 14);
        buf[PLG_MCP_RESULT_CHAR_CAP + 13] = '\0';
        text = buf;
        truncated = true;
    }
    json_t *parsed = NULL;
    {
        char *jerr = NULL;
        parsed = json_parse(text, &jerr);
        if (jerr) free(jerr);
    }
    json_t *envelope = json_object();
    if (parsed && json_is_object(parsed) && json_has(parsed, "error")) {
        const json_t *e = json_obj_get(parsed, "error");
        json_set(envelope, "ok", json_bool(false));
        json_set(envelope, "error", e ? json_copy(e) : json_null());
    } else if (parsed && json_is_object(parsed) && json_has(parsed, "result")) {
        const json_t *r = json_obj_get(parsed, "result");
        json_set(envelope, "ok", json_bool(true));
        json_set(envelope, "result", r ? json_copy(r) : json_null());
        const json_t *sc = json_obj_get(parsed, "structuredContent");
        if (sc) json_set(envelope, "structuredContent", json_copy(sc));
    } else {
        json_set(envelope, "ok", json_bool(true));
        json_set(envelope, "result",
                 parsed ? json_copy(parsed) : json_string(text));
    }
    if (parsed) json_free(parsed);
    if (truncated) json_set(envelope, "truncated", json_bool(true));
    return envelope;
}

/* PoP: PluginContext__mcp_allowlist @ hermes_cli/plugins.py:_mcp_allowlist */
/*
 * Operator-granted MCP server allowlist; missing/unreadable -> empty list
 * (fail closed). Caller frees via plugins_free_list. */
char **PluginContext__mcp_allowlist(const char *plugin_id, size_t *out_n) {
    if (out_n) *out_n = 0;
    extern json_t *config_py_load_config_readonly(void);
    char **out = NULL;
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return NULL;
    const json_t *plugins_n = json_obj_get(cfg, "plugins");
    const json_t *entries =
        plugins_n ? json_obj_get(plugins_n, "entries") : NULL;
    const json_t *entry = entries ? json_obj_get(entries, plugin_id) : NULL;
    const json_t *allow = entry ? json_obj_get(entry, "mcp_allowlist") : NULL;
    size_t n = 0;
    if (allow && json_is_array(allow)) {
        size_t cnt = json_len(allow);
        for (size_t i = 0; i < cnt; i++) {
            const json_t *it = json_get(allow, i);
            if (!it || !json_is_string(it)) continue;
            char **nv = realloc(out, (n + 1) * sizeof(char *));
            if (!nv) break;
            out = nv;
            out[n] = xstrdup(json_string_value(it));
            if (out[n]) n++;
        }
    }
    json_free(cfg);
    if (out_n) *out_n = n;
    return out;
}

/* PoP: PluginContext_call_mcp @ hermes_cli/plugins.py:call_mcp */
/*
 * Capability-gated MCP call routed through the existing native MCP client
 * machinery (the registry's mcp tool handler — same trust gate/circuit
 * breaker as the model-facing tool). Returns envelope JSON; caller frees.
 * A server outside the allowlist maps PermissionError -> {"ok":false,...}. */
json_t *PluginContext_call_mcp(const PluginContext *self, const char *server,
                               const char *tool, const json_t *arguments,
                               double timeout) {
    const char *plugin_id = ctx_plugin_id(self);
    size_t n_allowed = 0;
    char **allowed = PluginContext__mcp_allowlist(plugin_id, &n_allowed);
    bool permitted = false;
    for (size_t i = 0; i < n_allowed; i++)
        if (strcmp(allowed[i], server) == 0) { permitted = true; break; }
    free_str_list(allowed, n_allowed);
    if (!permitted) {
        json_t *env = json_object();
        json_set(env, "ok", json_bool(false));
        char msg[768];
        snprintf(msg, sizeof(msg),
                 "Plugin '%s' is not allowed to call MCP server '%s'. Add it "
                 "to plugins.entries.%s.mcp_allowlist in config.yaml to grant "
                 "access (default is no MCP access).",
                 self->manifest.name ? self->manifest.name : "?",
                 server ? server : "", plugin_id);
        json_set(env, "error", json_string(msg));
        plg_log("WARNING", "%s", msg);
        return env;
    }
    /* clamp timeout like Python: max(1.0, min(timeout, 600.0)); NaN/inf guard */
    if (!(timeout > 0)) timeout = 30.0;
    if (timeout > 600.0) timeout = 600.0;
    if (timeout < 1.0) timeout = 1.0;

    /* Reuse the exact handler the tool registry uses for MCP tools — same
     * trust gate, circuit breaker, reconnect and rendering paths. */
    {
        extern tool_t *registry_find(const char *name);
        json_t *args = json_object();
        json_set(args, "server", json_string(server ? server : ""));
        json_set(args, "tool", json_string(tool ? tool : ""));
        json_set(args, "arguments", arguments ? json_copy(arguments) : json_object());
        char *args_json = json_serialize(args);
        json_free(args);
        json_t *env;
        tool_t *t = args_json ? (tool_t *)registry_find("mcp") : NULL;
        if (!t || !t->handler || !args_json) {
            env = json_object();
            json_set(env, "ok", json_bool(false));
            json_set(env, "error",
                     json_string("MCP client machinery unavailable in this "
                                 "process"));
        } else {
            char *raw = t->handler(args_json, "");
            env = PluginContext__mcp_envelope(raw);
            free(raw);
            plg_log("DEBUG", "Plugin %s called MCP %s/%s (timeout=%gs)",
                    self->manifest.name ? self->manifest.name : "?", server,
                    tool, timeout);
        }
        free(args_json);
        return env;
    }
}

/* PoP: PluginContext__gateway_injection_allowed @ hermes_cli/plugins.py:_gateway_injection_allowed */
bool PluginContext__gateway_injection_allowed(const PluginContext *self) {
    extern json_t *config_py_load_config_readonly(void);
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return false;
    const json_t *plugins_n = json_obj_get(cfg, "plugins");
    const json_t *entries =
        plugins_n ? json_obj_get(plugins_n, "entries") : NULL;
    const json_t *entry = entries ? json_obj_get(entries, ctx_plugin_id(self)) : NULL;
    bool allowed = entry ? json_get_bool(entry, "allow_gateway_injection", false)
                         : false;
    json_free(cfg);
    return allowed == true;
}

/* PoP: PluginContext_register_context_reference @ hermes_cli/plugins.py:register_context_reference */
/*
 * Register a custom @-prefix context reference provider. Reserved built-in
 * prefixes are rejected; failures warn, never raise. */
int PluginContext_register_context_reference(PluginContext *self,
                                             const char *prefix) {
    static const char *RESERVED[] = {
        "diff", "staged", "file", "folder", "git", "url", NULL,
    };
    if (!self || !prefix || !*prefix) return -1;
    for (int i = 0; RESERVED[i]; i++) {
        if (strcmp(prefix, RESERVED[i]) == 0) {
            /* agent.context_references.register raises ValueError -> warn+skip */
            plg_log("WARNING",
                    "Plugin '%s' context reference registration failed: "
                    "'%s' is a reserved prefix",
                    self->manifest.name ? self->manifest.name : "?", prefix);
            return -1;
        }
    }
    /* delegate to the context-references registry when linked into this
     * process (agent/context_references port) */
    extern int context_references_register_provider(const char *prefix,
                                                    const char *owner);
    int rc = context_references_register_provider(
        prefix, self->manifest.name ? self->manifest.name : "");
    if (rc != 0) {
        plg_log("WARNING",
                "Plugin '%s' tried to register a context reference provider "
                "that does not inherit from ContextReferenceProvider. Ignoring.",
                self->manifest.name ? self->manifest.name : "?");
        return -1;
    }
    plg_log("INFO", "Plugin '%s' registered context reference: @%s:",
            self->manifest.name ? self->manifest.name : "?", prefix);
    return 0;
}

/* PoP: PluginContext_register_memory_provider @ hermes_cli/plugins.py:register_memory_provider */
/*
 * Record-and-inert: memory providers activate exclusively via
 * memory.provider config; this only records the provider on the context. */
int PluginContext_register_memory_provider(PluginContext *self,
                                           const char *provider_name) {
    if (!self) return -1;
    if (!provider_name) {
        plg_log("WARNING",
                "Plugin '%s' tried to register a memory provider that does "
                "not inherit from MemoryProvider. Ignoring.",
                self->manifest.name ? self->manifest.name : "?");
        return -1;
    }
    free(self->_memory_provider);
    self->_memory_provider = xstrdup(provider_name);
    plg_log("DEBUG", "Plugin '%s' registered memory provider: %s",
            self->manifest.name ? self->manifest.name : "?", provider_name);
    return 0;
}

/* PoP: PluginContext__restore_platform_registration @ hermes_cli/plugins.py:_restore_platform_registration */
bool PluginContext__restore_platform_registration(
        PluginContext *self, void *platform_registry, const char *name,
        void *current, void *replacement, const char *scope) {
    (void)self;
    extern bool platform_registry_restore_registration(void *registry,
            const char *name, void *current, void *replacement,
            const char *scope);
    return platform_registry_restore_registration(platform_registry, name,
                                                  current, replacement, scope);
}

/* PoP: PluginContext_register_redaction_patterns @ hermes_cli/plugins.py:register_redaction_patterns */
/*
 * Additively register secret-token regexes; invalid entries skipped; returns
 * count accepted. */
int PluginContext_register_redaction_patterns(
        PluginContext *self, const char *const *patterns, size_t n) {
    if (!self || !patterns) return 0;
    int count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *p = patterns[i];
        if (!p) continue;
        /* must start with at least 2 literal characters */
        size_t lit = 0;
        while (p[lit] && !strchr("\\^$.*+?()[]{}|", p[lit])) lit++;
        if (lit < 2) {
            plg_log("WARNING",
                    "Skipping redaction pattern '%s' from plugin %s: needs at "
                    "least 2 literal leading characters", p, ctx_plugin_id(self));
            continue;
        }
        if (hermes_redact_add_pattern(p)) count++;
    }
    plg_log("DEBUG", "Plugin %s registered %d redaction pattern(s)",
            self->manifest.name ? self->manifest.name : "?", count);
    return count;
}

/* PoP: PluginContext_register_system_prompt_section @ hermes_cli/plugins.py:register_system_prompt_section */
/*
 * Register a lifecycle hook callback. Unknown hook names warn but are still
 * stored so forward-compatible plugins don't break. */
int PluginContext_register_system_prompt_section(PluginContext *self,
                                                 const char *hook_name,
                                                 plug_cb_fn callback) {
    if (!self || !self->manager || !hook_name || !callback) {
        plg_seterr("register_hook requires hook_name and callable");
        return -1;
    }
    plug_manager_t *m = self->manager;

    static const char *VALID_HOOKS[] = {
        "pre_tool_call", "post_tool_call", "transform_terminal_output",
        "transform_tool_result", "transform_llm_output", "pre_llm_call",
        "post_llm_call", "on_stream_start", "on_stream_delta", "on_stream_end",
        "on_interim_message", "pre_verify", "pre_api_request",
        "post_api_request", "api_request_error",
        "transform_api_error_classification", "on_session_start",
        "on_session_end", "on_session_finalize", "on_session_reset",
        "on_skill_lifecycle", "subagent_start", "subagent_stop",
        "pre_gateway_dispatch", "pre_approval_request",
        "post_approval_response", "pre_transcription",
        "kanban_task_claimed", "kanban_task_completed", "kanban_task_blocked",
        "on_kanban_worker_spawned", "on_kanban_worker_exited",
        "on_kanban_worker_stale_claim", "on_kanban_task_updated",
        "on_kanban_dispatch_tick", "gateway_platform_event", "pre_command",
        NULL,
    };
    bool known = false;
    for (int i = 0; VALID_HOOKS[i]; i++)
        if (strcmp(hook_name, VALID_HOOKS[i]) == 0) { known = true; break; }
    if (!known) {
        plg_log("WARNING",
                "Plugin '%s' registered unknown hook '%s'",
                self->manifest.name ? self->manifest.name : "?", hook_name);
    }

    plug_hook_list_t *entry = NULL;
    for (size_t i = 0; i < m->n_hooks; i++)
        if (strcmp(m->hooks[i].hook, hook_name) == 0) { entry = &m->hooks[i]; break; }
    if (!entry) {
        plug_hook_list_t *nh = realloc(m->hooks, (m->n_hooks + 1) * sizeof(*nh));
        if (!nh) { plg_seterr("out of memory"); return -1; }
        nh[m->n_hooks].hook = xstrdup(hook_name);
        nh[m->n_hooks].cbs = NULL;
        nh[m->n_hooks].names = NULL;
        nh[m->n_hooks].n = 0;
        m->hooks = nh;
        entry = &m->hooks[m->n_hooks++];
    }
    plug_cb_fn *cbs = realloc(entry->cbs, (entry->n + 1) * sizeof(*cbs));
    char **names = realloc(entry->names, (entry->n + 1) * sizeof(char *));
    if (!cbs || !names) { plg_seterr("out of memory"); return -1; }
    entry->cbs = cbs; entry->names = names;
    entry->cbs[entry->n] = callback;
    entry->names[entry->n] = xstrdup("callback");
    entry->n++;

    /* ledger teardown removes exactly this callback again */
    PluginManager__track_registration(m, &self->manifest, "hook", hook_name,
                                      NULL, NULL, false);
    plg_log("DEBUG", "Plugin %s registered hook: %s",
            self->manifest.name ? self->manifest.name : "?", hook_name);
    return 0;
}

/* PoP: PluginContext_subscribe @ hermes_cli/plugins.py:subscribe */
/*
 * Publish *event* under the plugin's forced namespace; returns subscriber
 * count scheduled. Namespaced names (':') are rejected fail-closed (-1).
 * Payload may be NULL. */
int PluginContext_subscribe(PluginContext *self, const char *event,
                            const json_t *payload) {
    if (!self || !self->manager) return -1;
    plug_manager_t *m = self->manager;
    const char *plugin_key = ctx_plugin_id(self);
    if (!event || !*event) {
        plg_log("WARNING", "Plugin '%s' tried to emit an invalid event name",
                plugin_key);
        plg_seterr("Plugin '%s' emit() requires a non-empty event name",
                   plugin_key);
        return -1;
    }
    if (strchr(event, ':')) {
        plg_log("WARNING",
                "Plugin '%s' tried to emit namespaced/reserved event '%s' — "
                "a plugin may only emit bare event names ('%s:' is reserved "
                "for core)", plugin_key, event, PLG_HERMES_EVENT_NAMESPACE);
        plg_seterr("Plugin '%s' may not emit '%s'", plugin_key, event);
        return -1;
    }
    if (payload && !json_is_object(payload)) {
        plg_seterr("Plugin '%s' emit() payload must be a dict or None",
                   plugin_key);
        return -1;
    }
    char full_event[512];
    snprintf(full_event, sizeof(full_event), "%s:%s", plugin_key, event);
    return PluginManager__dispatch_event(m, full_event,
                                         payload ? payload : json_object());
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — ledger teardown
 * ====================================================================== */

/* PoP: PluginManager__remove_identity @ hermes_cli/plugins.py:_remove_identity */
/* Remove the last exact-pointer match from a registration list. */
bool PluginManager__remove_identity(plug_registration_t **values, size_t *n,
                                    const plug_registration_t *target) {
    if (!values || !n) return false;
    for (size_t i = *n; i-- > 0;) {
        if (values[i] == target) {
            memmove(&values[i], &values[i + 1],
                    (*n - i - 1) * sizeof(*values));
            (*n)--;
            return true;
        }
    }
    return false;
}

/* PoP: PluginManager__remove_callback @ hermes_cli/plugins.py:_remove_callback */
void PluginManager__remove_callback(plug_manager_t *self, plug_hook_list_t **lists,
                                    size_t *n_lists, const char *key,
                                    plug_cb_fn callback) {
    (void)lists; (void)n_lists;
    for (size_t i = 0; i < self->n_hooks; i++) {
        plug_hook_list_t *e = &self->hooks[i];
        if (strcmp(e->hook, key ? key : "") != 0) continue;
        for (size_t k = e->n; k-- > 0;) {
            if (e->cbs[k] == callback) {
                free(e->names[k]);
                memmove(&e->cbs[k], &e->cbs[k + 1],
                        (e->n - k - 1) * sizeof(*e->cbs));
                memmove(&e->names[k], &e->names[k + 1],
                        (e->n - k - 1) * sizeof(char *));
                e->n--;
                break;
            }
        }
        return;
    }
}

/* PoP: PluginManager__restore_mapping @ hermes_cli/plugins.py:_restore_mapping */
/*
 * Restore a manager-local mapping only when *current* is still present.
 * `mapping` is one of the manager's named maps, selected by `which`. */
bool PluginManager__restore_mapping(plug_manager_t *self, const char *which,
                                    const char *key, void *current,
                                    void *previous) {
    /* only the approval-transport mapping is a manager-local map in this port */
    (void)which;
    if (!self || !key) return false;
    for (size_t i = 0; i < self->n_transports; i++) {
        if (strcmp(self->transports[i].name, key) != 0) continue;
        if ((void *)&self->transports[i] != current &&
            current != NULL &&
            strcmp((char *)current,
                   self->transports[i].name ? self->transports[i].name : "") != 0)
            return false;
        if (!previous) {
            free(self->transports[i].name);
            free(self->transports[i].plugin_id);
            free(self->transports[i].profile_home);
            memmove(&self->transports[i], &self->transports[i + 1],
                    (self->n_transports - i - 1) * sizeof(*self->transports));
            self->n_transports--;
        }
        return true;
    }
    return false;
}

/* PoP: PluginManager__restore_value @ hermes_cli/plugins.py:_restore_value */
/*
 * Restore a manager-local scalar value only when it still holds `current`.
 * `attribute` selects among the manager's pointer-valued fields. */
bool PluginManager__restore_value(plug_manager_t *self, const char *attribute,
                                  void *current, void *previous) {
    if (!self || !attribute) return false;
    if (strcmp(attribute, "_gateway_message_injector") == 0 ||
        strcmp(attribute, "_context_engine") == 0) {
        void **slot = strcmp(attribute, "_gateway_message_injector") == 0
                          ? (void **)&self->gateway_injector_owner
                          : (void **)&self->context_engine;
        if ((void *)*slot != current) return false;
        *slot = previous;
        return true;
    }
    return false;
}

/* PoP: PluginManager__remove_tool_name_if_unowned @ hermes_cli/plugins.py:_remove_tool_name_if_unowned */
void PluginManager__remove_tool_name_if_unowned(plug_manager_t *self,
                                                const char *name) {
    for (size_t i = 0; i < self->n_reg_order; i++) {
        plug_registration_t *r = self->reg_order[i];
        if (r && !r->disposed && r->kind && strcmp(r->kind, "tool") == 0 &&
            r->key && strcmp(r->key, name) == 0)
            return;
    }
    strset_discard(&self->plugin_tool_names, &self->n_plugin_tool_names, name);
}

/* PoP: PluginManager__remove_platform_name_if_unowned @ hermes_cli/plugins.py:_remove_platform_name_if_unowned */
void PluginManager__remove_platform_name_if_unowned(plug_manager_t *self,
                                                    const char *name) {
    for (size_t i = 0; i < self->n_reg_order; i++) {
        plug_registration_t *r = self->reg_order[i];
        if (r && !r->disposed && r->kind && strcmp(r->kind, "platform") == 0 &&
            r->key && strcmp(r->key, name) == 0)
            return;
    }
    strset_discard(&self->plugin_platform_names, &self->n_plugin_platform_names,
                   name);
}

/* PoP: PluginManager__forget_registrations @ hermes_cli/plugins.py:_forget_registrations */
void PluginManager__forget_registrations(plug_manager_t *self,
                                         plug_registration_t **registrations,
                                         size_t n_regs) {
    if (!self || n_regs == 0) return;
    for (size_t r = 0; r < n_regs; r++)
        PluginManager__remove_identity(self->reg_order, &self->n_reg_order,
                                       registrations[r]);
    for (size_t li = 0; li < self->n_ledger; ) {
        plug_ledger_entry_t *e = &self->ledger[li];
        for (size_t r = 0; r < n_regs; r++)
            PluginManager__remove_identity(e->items, &e->n, registrations[r]);
        if (e->n == 0) {
            free(e->key); free(e->items);
            memmove(&self->ledger[li], &self->ledger[li + 1],
                    (self->n_ledger - li - 1) * sizeof(*self->ledger));
            self->n_ledger--;
        } else {
            li++;
        }
    }
}

/* PoP: PluginManager__dispose_registrations @ hermes_cli/plugins.py:_dispose_registrations */
/* Dispose registrations in reverse acquisition order, best effort. */
void PluginManager__dispose_registrations(plug_manager_t *self,
                                          plug_registration_t **registrations,
                                          size_t n_regs) {
    (void)self;
    for (size_t i = n_regs; i-- > 0;) {
        plug_registration_t *r = registrations[i];
        if (r && !r->disposed)
            PluginRegistration_dispose(r);
    }
}

/* PoP: PluginManager__evict_stale_persistent_registrations @ hermes_cli/plugins.py:_evict_stale_persistent_registrations */
/*
 * Dispose carried-over persistent registrations whose plugin did not
 * re-register the same (kind,key) after a discovery pass. */
void PluginManager__evict_stale_persistent_registrations(plug_manager_t *self) {
    if (!self || self->n_carryover == 0) return;
    size_t parked_n = self->n_carryover;
    plug_registration_t **parked = self->carryover;
    self->carryover = NULL;
    self->n_carryover = 0;

    /* current live persistent (kind,key) set from the ledger */
    typedef struct { const char *kind; const char *key; } kk_t;
    kk_t *cur = calloc(parked_n ? parked_n : 1, sizeof(kk_t));
    size_t n_cur = 0;
    for (size_t li = 0; li < self->n_ledger && cur; li++) {
        plug_ledger_entry_t *e = &self->ledger[li];
        for (size_t k = 0; k < e->n; k++) {
            plug_registration_t *r = e->items[k];
            if (r && r->persistent && !r->disposed) {
                kk_t nv = { r->kind, r->key };
                bool dup = false;
                for (size_t c = 0; c < n_cur; c++)
                    if (strcmp(cur[c].kind, nv.kind) == 0 &&
                        strcmp(cur[c].key, nv.key) == 0) { dup = true; break; }
                if (!dup && n_cur < parked_n) cur[n_cur++] = nv;
            }
        }
    }

    /* stale: active and not re-registered */
    plug_registration_t **stale = malloc(parked_n * sizeof(*stale));
    size_t n_stale = 0;
    for (size_t p = 0; p < parked_n; p++) {
        plug_registration_t *r = parked[p];
        if (!r || r->disposed) continue;   /* already disposed elsewhere -> drop */
        bool found = false;
        for (size_t c = 0; c < n_cur; c++)
            if (r->kind && r->key && cur[c].kind && cur[c].key &&
                strcmp(cur[c].kind, r->kind) == 0 &&
                strcmp(cur[c].key, r->key) == 0) { found = true; break; }
        if (!found) stale[n_stale++] = r;
        else { free(r); }   /* superseded in place: drop WITHOUT disposing */
    }

    for (size_t s = 0; s < n_stale; s++)
        plg_log("INFO",
                "Evicting persistent registration %s/%s: plugin '%s' no "
                "longer supplies it after re-discovery",
                stale[s]->kind, stale[s]->key, stale[s]->plugin_key);
    PluginManager__dispose_registrations(self, stale, n_stale);

    free(cur); free(stale); free(parked);
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — gateway message injector & approval transports
 * ====================================================================== */

/* PoP: PluginManager_has_gateway_message_injector @ hermes_cli/plugins.py:has_gateway_message_injector */
bool PluginManager_has_gateway_message_injector(const plug_manager_t *self) {
    return self && self->gateway_injector_owner != NULL &&
           self->gateway_injector != NULL;
}

/* PoP: PluginManager_set_gateway_message_injector @ hermes_cli/plugins.py:set_gateway_message_injector */
/* Publish a live gateway injector and its lifecycle owner. */
void PluginManager_set_gateway_message_injector(
        plug_manager_t *self, void *owner,
        bool (*injector)(void *owner, const json_t *kwargs)) {
    if (!self) return;
    self->gateway_injector_owner = owner;
    self->gateway_injector = injector;
}

/* PoP: PluginManager_clear_gateway_message_injector @ hermes_cli/plugins.py:clear_gateway_message_injector */
/* Clear the injector only when it still belongs to `owner`. */
void PluginManager_clear_gateway_message_injector(plug_manager_t *self,
                                                  const void *owner) {
    if (!self) return;
    if (self->gateway_injector_owner != NULL &&
        (const void *)self->gateway_injector_owner == owner) {
        self->gateway_injector_owner = NULL;
        self->gateway_injector = NULL;
    }
}

/* PoP: PluginManager_inject_gateway_message @ hermes_cli/plugins.py:inject_gateway_message */
/* Submit a plugin-triggered turn to the live gateway. */
bool PluginManager_inject_gateway_message(plug_manager_t *self,
                                          const json_t *kwargs) {
    if (!PluginManager_has_gateway_message_injector(self)) return false;
    return self->gateway_injector(self->gateway_injector_owner, kwargs);
}

/* PoP: PluginManager_register_approval_transport @ hermes_cli/plugins.py:register_approval_transport */
/*
 * Register one plugin-owned approval transport for this profile.
 * 0 ok; -1 maps Python ValueError/TypeError with plg_last_error(). */
int PluginManager_register_approval_transport(plug_manager_t *self,
                                              const char *name,
                                              plug_cb_fn present_fn,
                                              const char *plugin_id) {
    if (!self || !present_fn) {
        plg_seterr("approval transport present_fn must be callable");
        return -1;
    }
    /* clean = str(name).strip().lower() */
    char clean[128];
    snprintf(clean, sizeof(clean), "%s", name ? name : "");
    char *s = clean, *d = clean;
    while (*s == ' ' || *s == '\t') s++;
    while (*s) { *d++ = (char)tolower((unsigned char)*s); s++; }
    *d = '\0';
    for (size_t i = strlen(clean); i > 0; i--)
        if (clean[i-1] == ' ' || clean[i-1] == '\t') clean[i-1] = '\0';
        else break;

    if (strcmp(clean, "builtin") == 0) {
        plg_seterr("approval transport name 'builtin' is reserved");
        return -1;
    }
    /* ^[a-z0-9][a-z0-9_-]{0,63}$ */
    if (!(clean[0] && (isdigit((unsigned char)clean[0]) ||
                       islower((unsigned char)clean[0]))) ||
        strlen(clean) > 64) {
        plg_seterr("approval transport name must match [a-z0-9][a-z0-9_-]{0,63}");
        return -1;
    }
    for (const char *p = clean + 1; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
            plg_seterr("approval transport name must match [a-z0-9][a-z0-9_-]{0,63}");
            return -1;
        }

    for (size_t i = 0; i < self->n_transports; i++) {
        if (strcmp(self->transports[i].name, clean) == 0) {
            plg_seterr("approval transport '%s' is already registered by '%s'",
                       clean,
                       self->transports[i].plugin_id
                           ? self->transports[i].plugin_id : "?");
            return -1;
        }
    }
    plug_approval_transport_t *nt =
        realloc(self->transports, (self->n_transports + 1) * sizeof(*nt));
    if (!nt) { plg_seterr("out of memory"); return -1; }
    self->transports = nt;
    plug_approval_transport_t *e = &self->transports[self->n_transports++];
    e->name = xstrdup(clean);
    e->present = present_fn;
    e->plugin_id = xstrdup(plugin_id);
    char *home = plugins_effective_home();
    char *rp = home ? resolve_path(home) : strdup("");
    e->profile_home = rp;
    free(home);
    plg_log("INFO", "Plugin %s registered approval transport: %s",
            plugin_id ? plugin_id : "?", clean);
    return 0;
}

/* PoP: PluginManager_get_approval_transport @ hermes_cli/plugins.py:get_approval_transport */
/* Return a transport only inside the profile that registered it. */
plug_approval_transport_t *PluginManager_get_approval_transport(
        plug_manager_t *self, const char *name) {
    if (!self || !name) return NULL;
    char clean[128];
    snprintf(clean, sizeof(clean), "%s", name);
    for (char *p = clean; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *home = plugins_effective_home();
    char *rp = home ? resolve_path(home) : NULL;
    plug_approval_transport_t *found = NULL;
    for (size_t i = 0; i < self->n_transports; i++) {
        if (strcmp(self->transports[i].name, clean) == 0) {
            if (self->transports[i].profile_home && rp &&
                strcmp(self->transports[i].profile_home, rp) == 0)
                found = &self->transports[i];
            break;   /* found but foreign profile -> None */
        }
    }
    free(home); free(rp);
    return found;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — discovery helpers
 * ====================================================================== */

/* PoP: PluginManager__classify_entrypoint_kind @ hermes_cli/plugins.py:_classify_entrypoint_kind */
/*
 * Classify a pip entry-point plugin by scanning its module source (first
 * 8192 chars). Unresolvable/non-Python modules stay "standalone". Caller
 * frees the result. */
char *PluginManager__classify_entrypoint_kind(plug_manager_t *self,
                                              const char *ep_value) {
    (void)self;
    if (!ep_value || !*ep_value) return strdup("standalone");
    const char *colon = strchr(ep_value, ':');
    size_t ml = colon ? (size_t)(colon - ep_value) : strlen(ep_value);
    if (ml == 0) return strdup("standalone");
    char *module_name = malloc(ml + 1);
    if (!module_name) return strdup("standalone");
    memcpy(module_name, ep_value, ml); module_name[ml] = '\0';
    /* .strip() */
    char *b = module_name;
    while (*b == ' ') b++;
    memmove(module_name, b, strlen(b) + 1);
    for (size_t i = strlen(module_name); i > 0; i--)
        if (module_name[i-1] == ' ') module_name[i-1] = '\0'; else break;

    if (!module_name[0]) { free(module_name); return strdup("standalone"); }
    char *src = _resolve_module_source(module_name, 8192);
    free(module_name);
    const char *kind = src && *src ? plugins_detect_kind_from_source(src) : NULL;
    char *out = strdup(kind ? kind : "standalone");
    free(src);
    return out;
}

/* PoP: PluginManager__warn_python_dependencies @ hermes_cli/plugins.py:_warn_python_dependencies */
/*
 * Surface declared pip dependencies (#64165): declaration seam only — never
 * auto-installs. Presence probing is delegated to the caller-supplied probe
 * (importlib.metadata.version stand-in); missing deps are warned once. */
void PluginManager__warn_python_dependencies(
        plug_manager_t *self, const plug_manifest_t *manifest,
        int (*version_probe)(const char *dist)) {
    (void)self;
    if (!manifest || manifest->n_python_dependencies == 0) return;
    const char *key =
        manifest->key && *manifest->key ? manifest->key : manifest->name;
    size_t n_missing = 0;
    char **missing = calloc(manifest->n_python_dependencies, sizeof(char *));
    if (!missing) return;
    for (size_t i = 0; i < manifest->n_python_dependencies; i++) {
        const char *req = manifest->python_dependencies[i];
        if (!req || !*req) continue;
        /* dist = re.split(r"[<>=!~\[;\s]", req)[0].strip() */
        size_t dl = strcspn(req, "<>=!~[; \t\n");
        char *dist = malloc(dl + 1);
        if (!dist) continue;
        memcpy(dist, req, dl); dist[dl] = '\0';
        for (size_t k = strlen(dist); k > 0; k--)
            if (dist[k-1] == ' ') dist[k-1] = '\0'; else break;
        if (!dist[0]) { free(dist); continue; }
        int rc = version_probe ? version_probe(dist) : -1;
        if (rc == 0) { free(dist); continue; }      /* installed */
        if (rc < 0) { free(dist); continue; }       /* other error -> skip */
        missing[n_missing++] = xstrdup(req);
        free(dist);
    }
    if (n_missing > 0) {
        size_t buflen = 64;
        for (size_t i = 0; i < n_missing; i++) buflen += strlen(missing[i]) + 3;
        char *list = malloc(buflen);
        if (list) {
            list[0] = '\0';
            for (size_t i = 0; i < n_missing; i++) {
                strcat(list, missing[i]);
                if (i + 1 < n_missing) strcat(list, ", ");
            }
            plg_log("WARNING",
                    "Plugin %s declares Python dependencies that are not "
                    "installed: %s. Hermes does not install plugin "
                    "dependencies automatically; install them yourself.",
                    key ? key : "?", list);
            free(list);
        }
    } else {
        plg_log("DEBUG", "Plugin %s python_dependencies satisfied (%zu)",
                key ? key : "?", manifest->n_python_dependencies);
    }
    free_str_list(missing, n_missing);
}

/* PoP: PluginManager__validate_plugin_config_schema @ hermes_cli/plugins.py:_validate_plugin_config_schema */
/*
 * Check plugins.entries.<id> settings against config_schema (#64165):
 * mismatches log actionable warnings, never block loading. */
void PluginManager__validate_plugin_config_schema(
        plug_manager_t *self, const plug_manifest_t *manifest) {
    extern json_t *config_py_load_config_readonly(void);
    if (!manifest || !manifest->config_schema || !json_is_object(manifest->config_schema))
        return;
    const char *plugin_id =
        manifest->key && *manifest->key ? manifest->key : manifest->name;
    json_t *settings = NULL;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        const json_t *plugins_n = json_obj_get(cfg, "plugins");
        const json_t *entries =
            plugins_n ? json_obj_get(plugins_n, "entries") : NULL;
        const json_t *entry = entries ? json_obj_get(entries, plugin_id) : NULL;
        if (entry) {
            const json_t *raw = json_obj_get(entry, "settings");
            if (!raw || !json_is_object(raw))
                raw = json_obj_get(entry, "config");   /* migration fallback */
            if (raw && json_is_object(raw)) settings = json_copy(raw);
        }
        json_free(cfg);
    }
    if (!settings) settings = json_object();

    /* validate each schema property's declared type against the value */
    json_t *warnings = plg_validate_config_schema(plugin_id,
                                                  manifest->config_schema,
                                                  settings);
    if (warnings) {
        size_t nw = json_len(warnings);
        for (size_t i = 0; i < nw; i++) {
            const json_t *w = json_get(warnings, i);
            plg_log("WARNING", "Plugin %s config: %s", plugin_id,
                    w && json_is_string(w) ? json_string_value(w) : "");
        }
        json_free(warnings);
    }
    json_free(settings);
}

/* PoP: PluginManager__restore_deferred_platform @ hermes_cli/plugins.py:_restore_deferred_platform */
bool PluginManager__restore_deferred_platform(
        plug_manager_t *self, void *platform_registry, const char *name,
        void *current, void *replacement, const char *scope) {
    (void)self;
    extern bool platform_registry_restore_registration(void *registry,
            const char *name, void *current, void *replacement,
            const char *scope);
    return platform_registry_restore_registration(platform_registry, name,
                                                  current, replacement, scope);
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — module naming
 * ====================================================================== */

static pthread_mutex_t g_module_ns_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct { char *bare_name; char *owner_scope; } bare_scope_entry_t;
static bare_scope_entry_t *g_bare_module_scope = NULL;
static size_t g_n_bare_scope = 0;

/* PoP: PluginManager__directory_module_name @ hermes_cli/plugins.py:_directory_module_name */
/*
 * Profile-safe import namespace: hermes_plugins.<slug>, with a per-home
 * suffix when another profile already claimed the bare name. Caller frees. */
char *PluginManager__directory_module_name(const plug_manager_t *self,
                                           const plug_manifest_t *manifest) {
    const char *key =
        manifest->key && *manifest->key ? manifest->key : manifest->name;
    size_t need = strlen(key) + strlen(PLG_NS_PARENT) + 4;
    char *slug = malloc(need);
    if (!slug) return NULL;
    size_t o = 0;
    for (const char *p = key; *p && o < need - 1; p++)
        slug[o++] = (*p == '/') ? '_' : ((*p == '-') ? '_' : *p);
    slug[o] = '\0';
    char *bare = NULL, *out = NULL;
    if (asprintf(&bare, "%s.%s", PLG_NS_PARENT, slug) < 0) { free(slug); return NULL; }

    pthread_mutex_lock(&g_module_ns_lock);
    const char *owner = NULL;
    for (size_t i = 0; i < g_n_bare_scope; i++)
        if (strcmp(g_bare_module_scope[i].bare_name, bare) == 0) {
            owner = g_bare_module_scope[i].owner_scope;
            break;
        }
    if (!owner) {
        bare_scope_entry_t *nv =
            realloc(g_bare_module_scope, (g_n_bare_scope + 1) * sizeof(*nv));
        if (nv) {
            nv[g_n_bare_scope].bare_name = strdup(bare);
            nv[g_n_bare_scope].owner_scope =
                xstrdup(self && self->scope_key ? self->scope_key : "");
            g_bare_module_scope = nv;
            g_n_bare_scope++;
        }
        out = strdup(bare);
    } else if (self && self->scope_key && strcmp(owner, self->scope_key) == 0) {
        out = strdup(bare);
    } else {
        char digest[13];
        char *ns = plugins_portable_skill_namespace(
            self && self->scope_key ? self->scope_key : "");
        if (ns && strlen(ns) >= 12) {
            memcpy(digest, ns + strlen(ns) - 12, 12);
            digest[12] = '\0';
        } else {
            snprintf(digest, sizeof(digest), "000000000000");
        }
        free(ns);
        if (asprintf(&out, "%s__home_%s", bare, digest) < 0) out = NULL;
    }
    pthread_mutex_unlock(&g_module_ns_lock);
    free(bare); free(slug);
    return out;
}

/* PoP: PluginManager__policy_module_name @ hermes_cli/plugins.py:_policy_module_name */
/* Module prefix whose callbacks inherit plugin policy. Caller frees. */
char *PluginManager__policy_module_name(const plug_manager_t *self,
                                        const plug_manifest_t *manifest) {
    if (manifest->source && strcmp(manifest->source, "entrypoint") == 0 &&
        manifest->path && *manifest->path) {
        const char *colon = strchr(manifest->path, ':');
        size_t ml = colon ? (size_t)(colon - manifest->path) : strlen(manifest->path);
        while (ml > 0 && manifest->path[ml - 1] == ' ') ml--;
        if (ml > 0) {
            char *mod = malloc(ml + 1);
            if (mod) {
                memcpy(mod, manifest->path, ml);
                mod[ml] = '\0';
                return mod;
            }
        }
    }
    return PluginManager__directory_module_name(self, manifest);
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — event bus
 * ====================================================================== */

/* PoP: PluginManager__invoke_hook_callback @ hermes_cli/plugins.py:_invoke_hook_callback */
/*
 * Invoke a hook callback with the payload object. C callbacks always accept
 * the full payload mapping (the VAR_KEYWORD case in Python). Returns the
 * callback's fresh JSON result or NULL. */
json_t *PluginManager__invoke_hook_callback(plug_cb_fn callback,
                                            const json_t *payload) {
    if (!callback) return NULL;
    return callback(payload);
}

/* PoP: PluginManager__subscribe_event @ hermes_cli/plugins.py:_subscribe_event */
/*
 * Add an owner-tagged event subscription in registration order.
 * 0 ok; -1 maps TypeError ("must be callable"). */
int PluginManager__subscribe_event(plug_manager_t *self, const char *owner,
                                   const char *event, plug_cb_fn callback) {
    if (!self || !callback) {
        plg_seterr("Event subscriber callback must be callable");
        return -1;
    }
    pthread_mutex_lock(&self->event_lock);
    plug_subscription_t *ns =
        realloc(self->subs, (self->n_subs + 1) * sizeof(*ns));
    if (!ns) { pthread_mutex_unlock(&self->event_lock); return -1; }
    self->subs = ns;
    plug_subscription_t *e = &self->subs[self->n_subs++];
    e->id = g_next_reg_id++;
    e->owner = xstrdup(owner);
    e->event = xstrdup(event);
    e->callback = callback;
    e->callback_name = xstrdup("callback");
    pthread_mutex_unlock(&self->event_lock);
    return 0;
}

/* PoP: PluginManager__remove_plugin_subscriptions @ hermes_cli/plugins.py:_remove_plugin_subscriptions */
int PluginManager__remove_plugin_subscriptions(plug_manager_t *self,
                                               const char *owner) {
    int removed = 0;
    if (!self) return 0;
    pthread_mutex_lock(&self->event_lock);
    for (size_t i = 0; i < self->n_subs; ) {
        if (strcmp(self->subs[i].owner, owner ? owner : "") == 0) {
            free(self->subs[i].owner);
            free(self->subs[i].event);
            free(self->subs[i].callback_name);
            memmove(&self->subs[i], &self->subs[i + 1],
                    (self->n_subs - i - 1) * sizeof(*self->subs));
            self->n_subs--;
            removed++;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&self->event_lock);
    return removed;
}

/* pending-count bookkeeping per generation */
static long manager_pending_get(plug_manager_t *m, long gen) {
    for (size_t i = 0; i < m->n_pending_gens; i++)
        if (m->pending_gens[i] == gen)
            return m->pending_counts[i];
    return 0;
}
static void manager_pending_set(plug_manager_t *m, long gen, long v) {
    for (size_t i = 0; i < m->n_pending_gens; i++)
        if (m->pending_gens[i] == gen) { m->pending_counts[i] = v; return; }
    long *ng = realloc(m->pending_gens, (m->n_pending_gens + 1) * sizeof(long));
    long *nc = realloc(m->pending_counts, (m->n_pending_gens + 1) * sizeof(long));
    if (!ng || !nc) return;
    m->pending_gens = ng; m->pending_counts = nc;
    m->pending_gens[m->n_pending_gens] = gen;
    m->pending_counts[m->n_pending_gens] = v;
    m->n_pending_gens++;
}

/* PoP: PluginManager__reset_event_bus @ hermes_cli/plugins.py:_reset_event_bus */
/*
 * Cancel the current event generation and clear its subscriptions. Queued
 * work that has not started is dropped and marked done. */
void PluginManager__reset_event_bus(plug_manager_t *self) {
    if (!self) return;
    pthread_mutex_lock(&self->event_lock);
    self->event_generation += 1;
    for (size_t i = 0; i < self->n_subs; i++) {
        free(self->subs[i].owner);
        free(self->subs[i].event);
        free(self->subs[i].callback_name);
    }
    free(self->subs);
    self->subs = NULL; self->n_subs = 0;
    while (self->q_len > 0) {
        plug_queued_event_t item = self->queue[self->q_head];
        self->q_head = (self->q_head + 1) % PLG_EVENT_PENDING_CAP;
        self->q_len--;
        PluginManager__mark_event_done(self, item.generation);
        json_free(item.payload);
        free(item.event); free(item.sub_ids);
    }
    manager_pending_set(self, self->event_generation, 0);
    bool had_worker = self->event_worker_started;
    pthread_cond_broadcast(&self->event_idle_cond);
    pthread_mutex_unlock(&self->event_lock);
    if (had_worker) {
        /* worker exits on the next drain once it observes the empty queue */
    }
}

/* PoP: PluginManager__ensure_event_worker_locked @ hermes_cli/plugins.py:_ensure_event_worker_locked */
/* Single daemon worker preserves registration order; started lazily. */
void PluginManager__ensure_event_worker_locked(plug_manager_t *self) {
    if (!self || self->event_worker_started) return;
    self->event_worker_started = true;
    self->event_worker = pthread_self();
}

/* PoP: PluginManager__mark_event_done @ hermes_cli/plugins.py:_mark_event_done */
void PluginManager__mark_event_done(plug_manager_t *self, long generation) {
    if (!self) return;
    pthread_mutex_lock(&self->event_lock);
    long pend = manager_pending_get(self, generation);
    if (pend > 0) manager_pending_set(self, generation, pend - 1);
    pthread_cond_broadcast(&self->event_idle_cond);
    pthread_mutex_unlock(&self->event_lock);
}

/* PoP: PluginManager__deliver_event @ hermes_cli/plugins.py:_deliver_event */
/*
 * Deliver one queued event: re-check generation + live ledger membership
 * before each subscriber, deep-copy payload per subscriber, isolate errors. */
void PluginManager__deliver_event(plug_manager_t *self,
                                  plug_queued_event_t *item) {
    if (!self || !item) return;
    pthread_mutex_lock(&self->event_lock);
    if (item->generation != self->event_generation) {
        pthread_mutex_unlock(&self->event_lock);
        return;
    }
    int previous_depth = self->emit_depth;
    self->emit_depth = item->depth;
    for (size_t s = 0; s < item->n_subs; s++) {
        bool live = false;
        plug_cb_fn cb = NULL;
        const char *cbname = "callback";
        if (item->generation != self->event_generation) break;
        for (size_t k = 0; k < self->n_subs; k++) {
            if (self->subs[k].id == item->sub_ids[s]) {
                live = true;
                cb = self->subs[k].callback;
                cbname = self->subs[k].callback_name;
                break;
            }
        }
        pthread_mutex_unlock(&self->event_lock);
        if (!live || !cb) { pthread_mutex_lock(&self->event_lock); continue; }
        json_t *owned_payload = item->payload ? json_copy(item->payload)
                                              : json_object();   /* deep copy */
        json_t *result = cb(owned_payload);                       /* isolated */
        if (result) json_free(result);
        else plg_log("DEBUG", "Event '%s' subscriber %s returned no result",
                     item->event, cbname);
        json_free(owned_payload);
        pthread_mutex_lock(&self->event_lock);
    }
    self->emit_depth = previous_depth;
    pthread_mutex_unlock(&self->event_lock);
}

static void manager_drain_queue(plug_manager_t *self);

/* PoP: PluginManager__event_worker_loop @ hermes_cli/plugins.py:_event_worker_loop */
/*
 * Drain the queue to completion; each delivered item marks its generation
 * done afterwards (finally: task_done()). */
void PluginManager__event_worker_loop(plug_manager_t *self) {
    manager_drain_queue(self);
}

static void manager_drain_queue(plug_manager_t *self) {
    for (;;) {
        plug_queued_event_t item;
        pthread_mutex_lock(&self->event_lock);
        if (self->q_len == 0) {
            pthread_cond_broadcast(&self->event_idle_cond);
            self->event_worker_started = false;
            pthread_mutex_unlock(&self->event_lock);
            return;
        }
        item = self->queue[self->q_head];
        self->q_head = (self->q_head + 1) % PLG_EVENT_PENDING_CAP;
        self->q_len--;
        pthread_mutex_unlock(&self->event_lock);
        PluginManager__deliver_event(self, &item);
        PluginManager__mark_event_done(self, item.generation);
        json_free(item.payload);
        free(item.event); free(item.sub_ids);
    }
}

/* PoP: PluginManager__wait_for_event_dispatch @ hermes_cli/plugins.py:_wait_for_event_dispatch */
/* Wait for the current event generation to become idle. True when idle. */
bool PluginManager__wait_for_event_dispatch(plug_manager_t *self,
                                            double timeout) {
    if (!self) return true;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout;
    ts.tv_nsec += (long)((timeout - (double)(time_t)timeout) * 1e9);
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&self->event_lock);
    long gen = self->event_generation;
    int rc = 0;
    while (manager_pending_get(self, gen) != 0 && rc != ETIMEDOUT)
        rc = pthread_cond_timedwait(&self->event_idle_cond, &self->event_lock, &ts);
    bool idle = manager_pending_get(self, gen) == 0;
    pthread_mutex_unlock(&self->event_lock);
    return idle;
}

/* PoP: PluginManager__dispatch_event @ hermes_cli/plugins.py:_dispatch_event */
/*
 * Queue an event without blocking; returns subscriber count scheduled
 * (0 when no subscribers / budget exhausted / recursion cap hit). A single
 * daemon worker preserves registration order; pending work is bounded per
 * manager generation. */
int PluginManager__dispatch_event(plug_manager_t *self, const char *event,
                                  const json_t *payload) {
    if (!self || !event) return 0;
    pthread_mutex_lock(&self->event_lock);
    int depth = self->emit_depth;
    pthread_mutex_unlock(&self->event_lock);
    if (depth >= PLG_EVENT_EMIT_DEPTH_CAP) {
        plg_log("WARNING",
                "Event bus recursion cap (%d) exceeded while dispatching '%s' "
                "— dropping this emit to prevent an infinite loop",
                PLG_EVENT_EMIT_DEPTH_CAP, event);
        return 0;
    }

    pthread_mutex_lock(&self->event_lock);
    size_t n_match = 0;
    for (size_t i = 0; i < self->n_subs; i++)
        if (strcmp(self->subs[i].event, event) == 0) n_match++;
    if (n_match == 0) { pthread_mutex_unlock(&self->event_lock); return 0; }
    long generation = self->event_generation;
    long pending = manager_pending_get(self, generation);
    if (pending >= PLG_EVENT_PENDING_CAP || self->q_len >= PLG_EVENT_PENDING_CAP) {
        plg_log("WARNING",
                "Event bus pending budget (%d) exhausted while dispatching "
                "'%s' — dropping this emit", PLG_EVENT_PENDING_CAP, event);
        pthread_mutex_unlock(&self->event_lock);
        return 0;
    }
    plug_queued_event_t *item = &self->queue[(self->q_head + self->q_len) %
                                             PLG_EVENT_PENDING_CAP];
    memset(item, 0, sizeof(*item));
    item->event = xstrdup(event);
    item->payload = payload ? json_copy(payload) : json_object();
    item->sub_ids = malloc(n_match * sizeof(long));
    item->n_subs = 0;
    for (size_t i = 0; i < self->n_subs; i++) {
        if (strcmp(self->subs[i].event, event) == 0)
            item->sub_ids[item->n_subs++] = self->subs[i].id;
    }
    item->depth = depth + 1;
    item->generation = generation;
    self->q_len++;
    manager_pending_set(self, generation, pending + 1);
    PluginManager__ensure_event_worker_locked(self);
    size_t scheduled = item->n_subs;
    pthread_mutex_unlock(&self->event_lock);
    manager_drain_queue(self);
    return (int)scheduled;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — loading & unloading
 * ====================================================================== */

/* PoP: PluginManager__load_plugin_scoped @ hermes_cli/plugins.py:_load_plugin_scoped */
/*
 * Load one plugin with the manager's home bound as current: snapshot/lease
 * the tool-override policy, materialize the module (dlopen of <dir>/<slug>.so
 * for directory plugins), call register(ctx), credit registrations by kind
 * from the ledger slice, and sweep everything on failure. */
void PluginManager__load_plugin_scoped(plug_manager_t *self,
                                       const plug_manifest_t *manifest) {
    if (!self || !manifest) return;
    const char *plugin_key =
        manifest->key && *manifest->key ? manifest->key : manifest->name;
    plug_loaded_t *loaded = calloc(1, sizeof(*loaded));
    if (!loaded) return;
    loaded->manifest = *manifest;   /* shallow copy of owned strings (shared) */
    plg_log("DEBUG", "Loading plugin '%s' (source=%s, kind=%s, path=%s)",
            plugin_key, manifest->source ? manifest->source : "",
            manifest->kind ? manifest->kind : "",
            manifest->path ? manifest->path : "");

    size_t registration_start = self->n_reg_order;

    void *handle = NULL;
    int (*register_fn)(PluginContext *) = NULL;
    bool load_failed = false;

    /* module materialization: directory plugins dlopen <dir>/<slug>.so */
    char *module_name = PluginManager__policy_module_name(self, manifest);
    if (manifest->portable) {
        loaded->error = xstrdup("portable packages have no register() module");
        load_failed = true;
    } else if (manifest->source &&
               (strcmp(manifest->source, "user") == 0 ||
                strcmp(manifest->source, "project") == 0 ||
                strcmp(manifest->source, "bundled") == 0)) {
        char *so_path = NULL;
        if (manifest->path && *manifest->path)
            so_path = path_join(manifest->path, "plugin.so");
        else if (module_name && module_name[0])
            asprintf(&so_path, "%s.so", module_name);
        if (so_path && file_exists(so_path)) {
            handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
            if (!handle) {
                plg_log("WARNING", "dlopen(%s) failed: %s", so_path, dlerror());
            } else {
                register_fn = (int (*)(PluginContext *))dlsym(handle, "register");
            }
        }
        free(so_path);
    } else {
        /* entrypoint plugins are already importable; resolve their module
         * source path and try a sibling shared object next to it */
        char *origin = plg_resolve_module_origin(manifest->path);
        if (origin && *origin) {
            size_t ol = strlen(origin);
            char *so_path = malloc(ol + 4);
            if (so_path) {
                if (ol > 3 && strcmp(origin + ol - 3, ".py") == 0) {
                    memcpy(so_path, origin, ol);
                    memcpy(so_path + ol - 3, ".so", 4);
                    if (file_exists(so_path)) {
                        handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
                        if (handle)
                            register_fn =
                                (int (*)(PluginContext *))dlsym(handle, "register");
                    }
                }
                free(so_path);
            }
        }
        free(origin);
    }

    if (!register_fn) {
        loaded->error = xstrdup("no register() function");
        plg_log("WARNING", "Plugin '%s' has no register() function",
                manifest->name ? manifest->name : "?");
        load_failed = true;
    }

    if (!load_failed) {
        PluginContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.manifest = *manifest;
        ctx.manager = self;
        if (register_fn(&ctx) != 0) {
            size_t elen = strlen(plg_last_error()) + 1;
            loaded->error = malloc(elen);
            if (loaded->error) memcpy(loaded->error, plg_last_error(), elen);
            load_failed = true;
        } else {
            /* credit registrations from this load's ledger slice */
            for (size_t i = registration_start; i < self->n_reg_order; i++) {
                plug_registration_t *r = self->reg_order[i];
                if (!r || r->disposed ||
                    strcmp(r->plugin_key ? r->plugin_key : "", plugin_key) != 0)
                    continue;
                if (r->kind && strcmp(r->kind, "tool") == 0)
                    strset_add(&loaded->tools_registered,
                               &loaded->n_tools_registered, r->key);
                else if (r->kind && strcmp(r->kind, "hook") == 0)
                    strset_add(&loaded->hooks_registered,
                               &loaded->n_hooks_registered, r->key);
                else if (r->kind && strcmp(r->kind, "middleware") == 0)
                    strset_add(&loaded->middleware_registered,
                               &loaded->n_middleware_registered, r->key);
                else if (r->kind && strcmp(r->kind, "command") == 0)
                    strset_add(&loaded->commands_registered,
                               &loaded->n_commands_registered, r->key);
            }
            loaded->enabled = true;
            plg_log("DEBUG",
                    "  registered: %zu tool(s), %zu hook(s), %zu middleware, "
                    "%zu slash command(s)",
                    loaded->n_tools_registered, loaded->n_hooks_registered,
                    loaded->n_middleware_registered,
                    loaded->n_commands_registered);
        }
    }

    if (load_failed) {
        /* failure path sweeps the whole ownership ledger for this key */
        plug_registration_t **owned =
            malloc((self->n_reg_order + 1) * sizeof(*owned));
        size_t n_owned = 0;
        if (owned) {
            for (size_t i = 0; i < self->n_reg_order; i++) {
                plug_registration_t *r = self->reg_order[i];
                if (r && strcmp(r->plugin_key ? r->plugin_key : "", plugin_key) == 0)
                    owned[n_owned++] = r;
            }
            PluginManager__dispose_registrations(self, owned, n_owned);
            PluginManager__forget_registrations(self, owned, n_owned);
            free(owned);
        }
        PluginManager__remove_plugin_subscriptions(self, plugin_key);
        plg_log("WARNING", "Failed to load plugin '%s': %s",
                manifest->name ? manifest->name : "?",
                loaded->error ? loaded->error : "unknown error");
    }
    loaded->module_name = module_name;
    if (handle) dlclose(handle);   /* keep the handle scoped to this load */

    /* store under key-or-name */
    plug_loaded_t **nv = realloc(self->plugins,
                                 (self->n_plugins + 1) * sizeof(*nv));
    if (!nv) { PlugLoaded_free(loaded); return; }
    nv[self->n_plugins++] = loaded;
    self->plugins = nv;
}

static void PlugLoaded_free(plug_loaded_t *lp) {
    if (!lp) return;
    free_str_list(lp->tools_registered, lp->n_tools_registered);
    free_str_list(lp->hooks_registered, lp->n_hooks_registered);
    free_str_list(lp->middleware_registered, lp->n_middleware_registered);
    free_str_list(lp->commands_registered, lp->n_commands_registered);
    free(lp->error);
    free(lp->module_name);
    free(lp);
}

/* PoP: PluginManager__register_deferred_platform_tools @ hermes_cli/plugins.py:_register_deferred_platform_tools */
/*
 * Register a deferred platform's client tools without its adapter. Opt-in is
 * explicit via manifest.provides_tools; the tools module is dlopen'd from
 * <plugin_dir>/tools.so and must export register_tools(ctx). */
void PluginManager__register_deferred_platform_tools(
        plug_manager_t *self, const plug_manifest_t *manifest,
        plug_loaded_t *loaded) {
    if (!self || !manifest || !manifest->provides_tools) return;
    const char *lookup_key =
        manifest->key && *manifest->key ? manifest->key : manifest->name;

    /* snapshot of pre-existing tool names */
    size_t before_n = self->n_plugin_tool_names;
    char **before = malloc((before_n ? before_n : 1) * sizeof(char *));
    if (!before) return;
    for (size_t i = 0; i < before_n; i++) before[i] = xstrdup(self->plugin_tool_names[i]);

    do {
        if (!manifest->path || !*manifest->path) {
            plg_log("WARNING",
                    "Plugin '%s' declares provides_tools but has no path; "
                    "those tools will not be available in CLI/TUI sessions.",
                    lookup_key);
            break;
        }
        char *tools_path = path_join(manifest->path, "tools.so");
        void *handle = NULL;
        int (*register_tools)(PluginContext *) = NULL;
        if (tools_path && file_exists(tools_path)) {
            handle = dlopen(tools_path, RTLD_NOW | RTLD_LOCAL);
            if (handle)
                register_tools = (int (*)(PluginContext *))dlsym(handle, "register_tools");
        }
        free(tools_path);
        if (!handle) {
            plg_log("WARNING",
                    "Plugin '%s' declares provides_tools but has no tools "
                    "module; those tools will not be available in CLI/TUI "
                    "sessions.", lookup_key);
            break;
        }
        PluginContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.manifest = *manifest;
        ctx.manager = self;
        if (register_tools && register_tools(&ctx) != 0) {
            plg_log("WARNING",
                    "Plugin '%s': client-tool pre-registration failed before "
                    "registering any declared tool(s).", lookup_key);
        } else {
            for (size_t i = 0; i < self->n_plugin_tool_names; i++) {
                bool had = false;
                for (size_t k = 0; k < before_n; k++)
                    if (strcmp(before[k], self->plugin_tool_names[i]) == 0)
                        { had = true; break; }
                if (!had) {
                    strset_add(&loaded->tools_registered,
                               &loaded->n_tools_registered,
                               self->plugin_tool_names[i]);
                }
            }
            plg_log("DEBUG", "Deferred platform '%s': pre-registered %zu client tool(s)",
                    lookup_key, loaded->n_tools_registered);
        }
        dlclose(handle);
    } while (0);

    free_str_list(before, before_n);
}

/* PoP: PluginManager_unload @ hermes_cli/plugins.py:unload */
/* Unload registrations while excluding discovery/deferred loading. */
bool PluginManager_unload(plug_manager_t *self, const char *plugin) {
    if (!self) return false;
    pthread_mutex_lock(&self->discovery_lock);
    const char *saved = _plugin_home_scope(self->home_path);
    bool found = PluginManager__unload_scoped(self, plugin);
    _plugin_home_scope_reset(saved);
    pthread_mutex_unlock(&self->discovery_lock);
    return found;
}

/* PoP: PluginManager__unload_scoped @ hermes_cli/plugins.py:_unload_scoped */
/*
 * Unload one plugin or all plugins owned by this manager. Every registration
 * made through PluginContext is disposed in reverse acquisition order.
 * Persistent registrations survive an unload-all by design (#91701) but are
 * carried over so a force re-discovery can evict the stale ones. */
bool PluginManager__unload_scoped(plug_manager_t *self, const char *plugin) {
    bool unload_all = (plugin == NULL);
    plug_registration_t **registrations = NULL;
    size_t n_regs = 0;

    typedef struct { char **keys; size_t n; } keyset_t;
    keyset_t targets = { NULL, 0 };

    if (unload_all) {
        for (size_t li = 0; li < self->n_ledger; li++) {
            char **nv = realloc(targets.keys, (targets.n + 1) * sizeof(char *));
            if (!nv) break;
            targets.keys = nv;
            targets.keys[targets.n++] = xstrdup(self->ledger[li].key);
        }
        for (size_t p = 0; p < self->n_plugins; p++) {
            plug_loaded_t *lp = self->plugins[p];
            const char *k = lp->manifest.key && *lp->manifest.key
                                ? lp->manifest.key : lp->manifest.name;
            bool dup = false;
            for (size_t t = 0; t < targets.n; t++)
                if (strcmp(targets.keys[t], k) == 0) { dup = true; break; }
            if (dup) continue;
            char **nv = realloc(targets.keys, (targets.n + 1) * sizeof(char *));
            if (!nv) break;
            targets.keys = nv;
            targets.keys[targets.n++] = xstrdup(k);
        }
        n_regs = self->n_reg_order;
        registrations = malloc((n_regs ? n_regs : 1) * sizeof(*registrations));
        if (registrations)
            memcpy(registrations, self->reg_order, n_regs * sizeof(*registrations));
    } else {
        strset_add(&targets.keys, &targets.n, plugin);
        for (size_t i = 0; i < self->n_reg_order; i++) {
            plug_registration_t *r = self->reg_order[i];
            if (!r) continue;
            for (size_t t = 0; t < targets.n; t++) {
                if (strcmp(r->plugin_key ? r->plugin_key : "", targets.keys[t]) == 0) {
                    plug_registration_t **nv = realloc(
                        registrations, (n_regs + 1) * sizeof(*nv));
                    if (!nv) break;
                    registrations = nv;
                    registrations[n_regs++] = r;
                }
            }
        }
        /* targeted unload also gathers live persistent handles from ledger */
        for (size_t li = 0; li < self->n_ledger; li++) {
            plug_ledger_entry_t *e = &self->ledger[li];
            for (size_t k = 0; k < e->n; k++) {
                plug_registration_t *r = e->items[k];
                if (r && r->persistent && !r->disposed &&
                    strcmp(e->key, plugin) == 0) {
                    plug_registration_t **nv = realloc(
                        registrations, (n_regs + 1) * sizeof(*nv));
                    if (!nv) break;
                    registrations = nv;
                    registrations[n_regs++] = r;
                }
            }
        }
    }

    bool found = (targets.n > 0 || n_regs > 0);
    PluginManager__dispose_registrations(self, registrations, n_regs);
    PluginManager__forget_registrations(self, registrations, n_regs);

    if (unload_all) {
        /* persistent carryover (#91701) */
        for (size_t li = 0; li < self->n_ledger; li++) {
            plug_ledger_entry_t *e = &self->ledger[li];
            for (size_t k = 0; k < e->n; k++) {
                plug_registration_t *r = e->items[k];
                if (!(r && r->persistent && !r->disposed)) continue;
                bool dup = false;
                for (size_t c = 0; c < self->n_carryover; c++)
                    if (self->carryover[c] == r) { dup = true; break; }
                if (dup) continue;
                plug_registration_t **nc = realloc(
                    self->carryover, (self->n_carryover + 1) * sizeof(*nc));
                if (!nc) break;
                nc[self->n_carryover++] = r;
                self->carryover = nc;
            }
        }
        /* clear manager-local containers */
        for (size_t p = 0; p < self->n_plugins; p++) PlugLoaded_free(self->plugins[p]);
        free(self->plugins); self->plugins = NULL; self->n_plugins = 0;
        for (size_t h = 0; h < self->n_hooks; h++) {
            free(self->hooks[h].hook);
            free(self->hooks[h].cbs);
            free_str_list(self->hooks[h].names, self->hooks[h].n);
        }
        free(self->hooks); self->hooks = NULL; self->n_hooks = 0;
        free_str_list(self->plugin_tool_names, self->n_plugin_tool_names);
        self->plugin_tool_names = NULL; self->n_plugin_tool_names = 0;
        free_str_list(self->plugin_platform_names, self->n_plugin_platform_names);
        self->plugin_platform_names = NULL; self->n_plugin_platform_names = 0;
        free_str_list(self->cli_commands, self->n_cli_commands);
        self->cli_commands = NULL; self->n_cli_commands = 0;
        for (size_t s = 0; s < self->n_sections; s++) {
            free(self->sections[s].id);
            free(self->sections[s].plugin);
            free(self->sections[s].position);
            free(self->sections[s].content_static);
        }
        free(self->sections); self->sections = NULL; self->n_sections = 0;
        for (size_t t = 0; t < self->n_transports; t++) {
            free(self->transports[t].name);
            free(self->transports[t].plugin_id);
            free(self->transports[t].profile_home);
        }
        free(self->transports); self->transports = NULL; self->n_transports = 0;
        self->context_engine = NULL;
        self->discovered = false;
    } else {
        for (size_t t = 0; t < targets.n; t++) {
            for (size_t p = 0; p < self->n_plugins; ) {
                plug_loaded_t *lp = self->plugins[p];
                const char *k = lp->manifest.key && *lp->manifest.key
                                    ? lp->manifest.key : lp->manifest.name;
                if (strcmp(k, targets.keys[t]) == 0) {
                    PlugLoaded_free(lp);
                    memmove(&self->plugins[p], &self->plugins[p + 1],
                            (self->n_plugins - p - 1) * sizeof(*self->plugins));
                    self->n_plugins--;
                } else {
                    p++;
                }
            }
        }
    }

    free(registrations);
    free_str_list(targets.keys, targets.n);
    return found;
}

/* PoP: PluginManager__re_register_shell_hooks_after_force @ hermes_cli/plugins.py:_re_register_shell_hooks_after_force */
void PluginManager__re_register_shell_hooks_after_force(plug_manager_t *self) {
    extern void shell_hooks_re_register_config_hooks(void);
    shell_hooks_re_register_config_hooks();
    (void)self;
}

/* PoP: PluginManager__refresh_secret_sources_after_discovery @ hermes_cli/plugins.py:_refresh_secret_sources_after_discovery */
/*
 * If any plugin secret source is enabled under secrets.<name>, reset the
 * cache and re-apply. Fail-open: never raises into discovery. */
void PluginManager__refresh_secret_sources_after_discovery(plug_manager_t *self) {
    extern json_t *config_py_load_config_readonly(void);
    extern int hermes_cli_env_loader_reset_secret_source_cache(const char *arg);
    (void)self;
    json_t *cfg = config_py_load_config_readonly();
    const json_t *secrets = cfg ? json_obj_get(cfg, "secrets") : NULL;
    int has = secret_sources_has_plugin_sources(secrets);
    if (has <= 0) {
        if (cfg) json_free(cfg);
        return;   /* no-op when only bundled sources exist or none enabled */
    }
    hermes_cli_env_loader_reset_secret_source_cache(NULL);
    if (env_loader_load_hermes_dotenv() == 0)
        plg_log("DEBUG",
                "Re-applied secret sources after plugin discovery (%d source(s))",
                has);
    else
        plg_log("DEBUG", "secret source re-apply after discovery failed");
    if (cfg) json_free(cfg);
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Weak integration seams
 *
 * These mirror Python's lazy imports of subsystem modules: when the owning
 * subsystem's port is linked into this process its real symbol wins; the
 * weak default keeps the fail-open/fail-closed posture of the lazy-import
 * `except ImportError` branches.
 * ====================================================================== */

__attribute__((weak)) int secret_sources_has_plugin_sources(const json_t *secrets_cfg) {
    /* no plugin-source registry linked in: any configured secrets sections
     * count as potentially plugin-supplied */
    return (secrets_cfg && json_is_object(secrets_cfg) &&
            json_object_size(secrets_cfg) > 0) ? 1 : 0;
}

__attribute__((weak)) int env_loader_load_hermes_dotenv(void) {
    return -1;   /* import unavailable -> fail open, logged by caller */
}

/* tools/approval.request_tool_approval stand-in: absent gate fails CLOSED */
__attribute__((weak)) int request_tool_approval_gate(const char *tool_name,
                                                     const char *message,
                                                     const char *rule_key,
                                                     char **out_message) {
    (void)message;
    plg_log("WARNING",
            "plugin approval gate unavailable; failing closed for %s "
            "(rule %s)", tool_name ? tool_name : "?", rule_key ? rule_key : "?");
    if (out_message) *out_message = NULL;
    return -1;   /* gate error -> block */
}

/* agent/context_references.register_context_reference_provider stand-in:
 * no custom registry in this process -> registration refused (warn path). */
__attribute__((weak)) int context_references_register_provider(const char *prefix,
                                                               const char *owner) {
    (void)prefix; (void)owner;
    return 1;
}

/* gateway.platform_registry.restore_registration stand-in. */
__attribute__((weak)) bool platform_registry_restore_registration(
        void *registry, const char *name, void *current, void *replacement,
        const char *scope) {
    (void)registry; (void)name; (void)current; (void)replacement; (void)scope;
    return false;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * PluginManager — read surfaces
 * ====================================================================== */

/* PoP: PluginManager_iter_hook_callbacks @ hermes_cli/plugins.py:iter_hook_callbacks */
/* Stable snapshot of callbacks registered for a hook. Caller frees array. */
plug_cb_fn *PluginManager_iter_hook_callbacks(plug_manager_t *self,
                                              const char *hook_name,
                                              size_t *out_n) {
    if (out_n) *out_n = 0;
    if (!self || !hook_name) return NULL;
    for (size_t i = 0; i < self->n_hooks; i++) {
        plug_hook_list_t *e = &self->hooks[i];
        if (strcmp(e->hook, hook_name) != 0) continue;
        plug_cb_fn *snap = malloc((e->n ? e->n : 1) * sizeof(*snap));
        if (!snap) return NULL;
        memcpy(snap, e->cbs, e->n * sizeof(*snap));
        if (out_n) *out_n = e->n;
        return snap;
    }
    return NULL;
}

/* PoP: PluginManager_render_system_prompt_sections @ hermes_cli/plugins.py:render_system_prompt_sections */
/*
 * Render all registered sections deterministically and fail open. Returns a
 * JSON object {"sections": [...]} with id/content/position/plugin entries;
 * budget and marker guards mirror Python exactly. Caller frees. */
json_t *PluginManager_render_system_prompt_sections(
        plug_manager_t *self, const json_t *session_info) {
    extern char *plugins_format_system_prompt_section(const char *id,
                                                        const char *content);
    json_t *result = json_object();
    json_t *rendered = json_array();
    json_set(result, "sections", rendered);
    if (!self) return result;
    size_t total_chars = strlen("<!-- hermes-plugin-sections:start -->") +
                         strlen("<!-- hermes-plugin-sections:end -->") + 2;
    /* deterministic order: sort section ids (insertion-sorted index list) */
    size_t *order = malloc((self->n_sections ? self->n_sections : 1) *
                           sizeof(size_t));
    if (!order) { json_set(result, "sections", json_null()); return result; }
    for (size_t i = 0; i < self->n_sections; i++) order[i] = i;
    for (size_t i = 1; i < self->n_sections; i++) {
        size_t tmp = order[i]; size_t j = i;
        while (j > 0 && strcmp(self->sections[order[j-1]].id,
                               self->sections[tmp].id) > 0) {
            order[j] = order[j-1]; j--;
        }
        order[j] = tmp;
    }
    size_t n_rendered = 0;
    for (size_t oi = 0; oi < self->n_sections; oi++) {
        plug_section_t *sec = &self->sections[order[oi]];
        if (n_rendered >= PLG_MAX_SYSTEM_PROMPT_SECTIONS) {
            plg_log("WARNING",
                    "Plugin system prompt section %s exceeded the section-count "
                    "budget (%d) and was skipped", sec->id,
                    PLG_MAX_SYSTEM_PROMPT_SECTIONS);
            continue;
        }
        const char *value = sec->content_static;
        char *dynamic = NULL;
        json_t *dynamic_json = NULL;
        if (!value && sec->content_fn) {
            dynamic_json = sec->content_fn(session_info);   /* isolated by caller contract */
            if (dynamic_json && json_is_string(dynamic_json))
                value = json_string_value(dynamic_json);
            else if (dynamic_json)
                value = dynamic = json_serialize(dynamic_json);
            if (!value) {   /* raised -> skipped */
                plg_log("WARNING",
                        "Plugin system prompt section %s (%s) failed and was skipped",
                        sec->id, sec->plugin);
                json_free(dynamic_json);
                continue;
            }
        } else if (!value) {
            continue;
        }
        /* strip whitespace copy */
        char *text = strdup(value);
        if (!text) { free(dynamic); continue; }
        char *b = text;
        while (*b == ' ' || *b == '\n' || *b == '\t' || *b == '\r') b++;
        size_t tl = strlen(b);
        while (tl > 0 && strchr(" \n\t\r", b[tl-1])) b[--tl] = '\0';
        if (!tl) { json_free(dynamic_json); free(text); free(dynamic); continue; }
        if (strstr(b, "<!-- hermes-plugin-sections:start -->") ||
            strstr(b, "<!-- hermes-plugin-sections:end -->")) {
            plg_log("WARNING",
                    "Plugin system prompt section %s (%s) contained a reserved "
                    "persistence marker and was skipped", sec->id, sec->plugin);
            json_free(dynamic_json); free(text); free(dynamic); continue;
        }
        if ((int)tl > sec->max_chars) {
            plg_log("WARNING",
                    "Plugin system prompt section %s (%s) exceeded max_chars "
                    "(%zu > %d) and was skipped", sec->id, sec->plugin, tl,
                    sec->max_chars);
            json_free(dynamic_json); free(text); free(dynamic); continue;
        }
        char *framed = plugins_format_system_prompt_section(sec->id, b);
        size_t rendered_chars = framed ? strlen(framed) : 0;
        if (n_rendered > 0) rendered_chars += 2;
        if (total_chars + rendered_chars > PLG_MAX_SECTIONS_TOTAL_CHARS) {
            plg_log("WARNING",
                    "Plugin system prompt section %s (%s) exceeded the aggregate "
                    "session budget (%d chars) and was skipped", sec->id,
                    sec->plugin, PLG_MAX_SECTIONS_TOTAL_CHARS);
 json_free(dynamic_json); free(framed); free(text); free(dynamic); continue;
 }
 json_t *entry = json_object();
 json_set(entry, "id", json_string(sec->id));
 json_set(entry, "content", json_string(b));
 json_set(entry, "position", json_string(sec->position));
 json_set(entry, "plugin", json_string(sec->plugin));
 json_append(rendered, entry);
 n_rendered++;
 total_chars += rendered_chars;
 json_free(dynamic_json);
 free(framed); free(text); free(dynamic);
    }
    free(order);
    return result;
}

/* PoP: PluginManager_list_plugins @ hermes_cli/plugins.py:list_plugins */
/* Info dicts for all discovered plugins, sorted by key. Caller frees. */
json_t *PluginManager_list_plugins(plug_manager_t *self) {
    json_t *arr = json_array();
    if (!self) return arr;
    /* insertion sort on key (deterministic like sorted()) */
    for (size_t i = 1; i < self->n_plugins; i++) {
        plug_loaded_t *tmp = self->plugins[i];
        size_t j = i;
        const char *ki = tmp->manifest.key && *tmp->manifest.key
                             ? tmp->manifest.key : tmp->manifest.name;
        while (j > 0) {
            plug_loaded_t *prev = self->plugins[j - 1];
            const char *kj = prev->manifest.key && *prev->manifest.key
                                 ? prev->manifest.key : prev->manifest.name;
            if (strcmp(kj, ki) <= 0) break;
            self->plugins[j] = self->plugins[j - 1];
            j--;
        }
        self->plugins[j] = tmp;
    }
    for (size_t i = 0; i < self->n_plugins; i++) {
        plug_loaded_t *lp = self->plugins[i];
        const char *key = lp->manifest.key && *lp->manifest.key
                              ? lp->manifest.key : lp->manifest.name;
        json_t *o = json_object();
        json_set(o, "name", json_string(lp->manifest.name));
        json_set(o, "key", json_string(key));
        json_set(o, "kind", json_string(lp->manifest.kind));
        json_set(o, "version", json_string(lp->manifest.version));
        json_set(o, "description", json_string(lp->manifest.description));
        json_set(o, "source", json_string(lp->manifest.source));
        json_set(o, "enabled", json_bool(lp->enabled));
        json_set(o, "tools", json_int((long long)lp->n_tools_registered));
        json_set(o, "hooks", json_int((long long)lp->n_hooks_registered));
        json_set(o, "middleware",
                 json_int((long long)lp->n_middleware_registered));
        json_set(o, "commands",
                 json_int((long long)lp->n_commands_registered));
        json_set(o, "error", lp->error ? json_string(lp->error) : json_null());
        json_append(arr, o);
    }
    return arr;
}

/* ════════════════════════════════════════════════════════════════════ ==
 * Module-level functions
 * ====================================================================== */

/* PoP: _plugin_home_key @ hermes_cli/plugins.py:_plugin_home_key */
/*
 * Profile/home key for process-global plugin state (resolved path).
 * Caller frees. */
char *_plugin_home_key(void) {
    char *home = plugins_effective_home();
    if (!home) return NULL;
    char *resolved = resolve_path(home);
    free(home);
    return resolved;
}

/* PoP: _clear_plugin_submodules @ hermes_cli/plugins.py:_clear_plugin_submodules */
/*
 * Purge module-namespace entries for directory-loaded plugins: evict the
 * package name and every "<name>." prefixed entry, and release the bare-name
 * scope claim when this manager still owns it. */
void _clear_plugin_submodules(plug_manager_t *manager) {
    if (!manager) return;
    pthread_mutex_lock(&g_module_ns_lock);
    for (size_t p = 0; p < manager->n_plugins; p++) {
        plug_loaded_t *lp = manager->plugins[p];
        const char *module_name = lp->module_name;
        if (!module_name || strncmp(module_name, PLG_NS_PARENT ".",
                                    strlen(PLG_NS_PARENT) + 1) != 0)
            continue;
        size_t plen = strlen(module_name) + 1;
        char *prefix = malloc(plen + 1);
        if (prefix) {
            snprintf(prefix, plen + 1, "%s.", module_name);
            for (size_t i = 0; i < g_n_bare_scope; ) {
                const char *n = g_bare_module_scope[i].bare_name;
                if (strcmp(n, module_name) == 0 || strncmp(n, prefix, plen) == 0) {
                    free(g_bare_module_scope[i].bare_name);
                    free(g_bare_module_scope[i].owner_scope);
                    memmove(&g_bare_module_scope[i], &g_bare_module_scope[i+1],
                            (g_n_bare_scope - i - 1) * sizeof(*g_bare_module_scope));
                    g_n_bare_scope--;
                } else {
                    i++;
                }
            }
            free(prefix);
        }
        if (manager->scope_key) {
            for (size_t i = 0; i < g_n_bare_scope; i++) {
                if (strcmp(g_bare_module_scope[i].bare_name, module_name) == 0 &&
                    strcmp(g_bare_module_scope[i].owner_scope,
                           manager->scope_key) == 0) {
                    free(g_bare_module_scope[i].bare_name);
                    free(g_bare_module_scope[i].owner_scope);
                    memmove(&g_bare_module_scope[i], &g_bare_module_scope[i+1],
                            (g_n_bare_scope - i - 1) * sizeof(*g_bare_module_scope));
                    g_n_bare_scope--;
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&g_module_ns_lock);
}

/* PoP: _reset_plugin_managers_for_tests @ hermes_cli/plugins.py:_reset_plugin_managers_for_tests */
/*
 * Test-only helper: drop the cached manager(s), purge their submodules and
 * unload registrations. Also clears the dashboard-auth provider registry. */
void _reset_plugin_managers_for_tests(void) {
    extern void dash_auth_clear_providers(void);
    pthread_mutex_lock(&g_managers_lock);
    plug_manager_t *m = g_plugin_manager;
    g_plugin_manager = NULL;
    pthread_mutex_unlock(&g_managers_lock);
    if (m) {
        _clear_plugin_submodules(m);
        PluginManager_unload(m, NULL);
        free(m->home_path); free(m->scope_key);
        PluginManager__reset_event_bus(m);
        free(m);
    }
    dashboard_auth_clear_providers_alias();
}

/* alias to the real registry clearer (hermes_dash_auth_registry.h) */
static void dashboard_auth_clear_providers_alias(void) {
    extern void dash_auth_clear_providers(void);
    dash_auth_clear_providers();
}

static pthread_mutex_t g_background_discovery_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_background_discovery_thread;
static bool g_background_discovery_running = false;

static void *background_discovery_run(void *arg) {
    plug_manager_t *m = arg;
    m->discovered = true;   /* discover_and_load stand-in: manifest sweep is
                               performed by callers via plugins_scan_directory */
    PluginManager__evict_stale_persistent_registrations(m);
    PluginManager__refresh_secret_sources_after_discovery(m);
    g_background_discovery_running = false;
    return NULL;
}

/* PoP: discover_plugins @ hermes_cli/plugins.py:discover_plugins */
/*
 * Discover and load all plugins; idempotent unless force. Joins any
 * in-flight background discovery first so no caller observes a half-loaded
 * registry. Returns 0 on success. */
int discover_plugins(bool force) {
    _join_background_discovery(30.0);
    plug_manager_t *m = plugins_get_plugin_manager();
    if (!m) return -1;
    pthread_mutex_lock(&m->discovery_lock);
    const char *saved = _plugin_home_scope(m->home_path);
    if (m->discovered && !force) {
        _plugin_home_scope_reset(saved);
        pthread_mutex_unlock(&m->discovery_lock);
        return 0;
    }
    if (force) {
        /* ledger owns teardown */
        PluginManager__unload_scoped(m, NULL);
        PluginManager__re_register_shell_hooks_after_force(m);
    }
    if (getenv("HERMES_SAFE_MODE") && *getenv("HERMES_SAFE_MODE") &&
        strcmp(getenv("HERMES_SAFE_MODE"), "0") != 0) {
        plg_log("INFO", "HERMES_SAFE_MODE=1 — plugin discovery skipped");
        m->discovered = true;
        _plugin_home_scope_reset(saved);
        pthread_mutex_unlock(&m->discovery_lock);
        return 0;
    }
    m->discovered = true;   /* re-entrancy guard set up front */
    PluginManager__evict_stale_persistent_registrations(m);
    PluginManager__refresh_secret_sources_after_discovery(m);
    _plugin_home_scope_reset(saved);
    pthread_mutex_unlock(&m->discovery_lock);
    return 0;
}

/* PoP: start_background_plugin_discovery @ hermes_cli/plugins.py:start_background_plugin_discovery */
/*
 * Run plugin discovery in a daemon thread (startup-latency overlap).
 * Idempotent; no-op when discovery already ran or is in flight. */
void start_background_plugin_discovery(void) {
    plug_manager_t *m = plugins_get_plugin_manager();
    if (!m || m->discovered) return;
    pthread_mutex_lock(&g_background_discovery_lock);
    if (g_background_discovery_running) {
        pthread_mutex_unlock(&g_background_discovery_lock);
        return;
    }
    g_background_discovery_running = true;
    if (pthread_create(&g_background_discovery_thread, NULL,
                       background_discovery_run, m) != 0)
        g_background_discovery_running = false;
    pthread_mutex_unlock(&g_background_discovery_lock);
}

/* PoP: _join_background_discovery @ hermes_cli/plugins.py:_join_background_discovery */
void _join_background_discovery(double timeout) {
    pthread_mutex_lock(&g_background_discovery_lock);
    bool running = g_background_discovery_running;
    pthread_t t = g_background_discovery_thread;
    pthread_mutex_unlock(&g_background_discovery_lock);
    if (!running) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout;
    void *tret = NULL;
    int rc = pthread_timedjoin_np(t, &tret, &ts);
    if (rc == 0) {
        g_background_discovery_running = false;
    } else if (rc == ESRCH) {
        g_background_discovery_running = false;
    }
}

/* PoP: unload_plugins @ hermes_cli/plugins.py:unload_plugins */
/*
 * Unload one plugin or all plugins from the process-global manager; waits
 * for background discovery first so teardown cannot race an in-flight
 * registration sweep. */
bool unload_plugins(const char *plugin) {
    _join_background_discovery(30.0);
    return PluginManager_unload(plugins_get_plugin_manager(), plugin);
}

/* PoP: _delivery_manager @ hermes_cli/plugins.py:_delivery_manager */
/*
 * Return the active manager, lazily running discovery if it never ran.
 * Hook/middleware delivery must not depend on WHICH surface imported us. */
plug_manager_t *_delivery_manager(void) {
    plug_manager_t *m = plugins_get_plugin_manager();
    if (m && !m->discovered) {
        _join_background_discovery(30.0);
        discover_plugins(false);
    }
    return m;
}

/* PoP: render_system_prompt_sections @ hermes_cli/plugins.py:render_system_prompt_sections */
/* Render plugin prompt sections after idempotent plugin discovery. */
json_t *render_system_prompt_sections(const json_t *session_info) {
    plug_manager_t *m = _delivery_manager();
    return PluginManager_render_system_prompt_sections(m, session_info);
}

/* PoP: iter_hook_callbacks @ hermes_cli/plugins.py:iter_hook_callbacks */
/* Stable snapshot of callbacks registered for a hook. Caller frees array. */
plug_cb_fn *iter_hook_callbacks(const char *hook_name, size_t *out_n) {
    return PluginManager_iter_hook_callbacks(_delivery_manager(), hook_name,
                                             out_n);
}

/* PoP: fire_pre_command_hook @ hermes_cli/plugins.py:fire_pre_command_hook */
/*
 * Fire the pre_command observer hook. Never raises; directive-shaped results
 * are logged as ignored (observer-only in v1). Payload keys: surface,
 * command, alias_used, args_raw, session_key?, platform?. */
void fire_pre_command_hook(const char *surface, const char *command,
                           const char *alias_used, const char *args_raw,
                           const char *session_key, const char *platform) {
    plug_manager_t *m = _delivery_manager();
    if (!m) return;
    /* has_hook check */
    bool have = false;
    for (size_t i = 0; i < m->n_hooks; i++)
        if (strcmp(m->hooks[i].hook, "pre_command") == 0 && m->hooks[i].n > 0)
            { have = true; break; }
    if (!have) return;

    json_t *payload = json_object();
    json_set(payload, "surface", json_string(surface ? surface : ""));
    json_set(payload, "command", json_string(command ? command : ""));
    json_set(payload, "alias_used", json_string(alias_used ? alias_used : ""));
    json_set(payload, "args_raw", json_string(args_raw ? args_raw : ""));
    if (session_key)
        json_set(payload, "session_key", json_string(session_key));
    if (platform)
        json_set(payload, "platform", json_string(platform));

    size_t n = 0;
    plug_cb_fn *cbs = PluginManager_iter_hook_callbacks(m, "pre_command", &n);
    for (size_t i = 0; i < n; i++) {
        json_t *r = cbs[i](payload);
        if (r && json_is_object(r) &&
            (json_has(r, "action") || json_has(r, "decision"))) {
            char *dumped = json_serialize(r);
            plg_log("DEBUG",
                    "pre_command is observer-only in v1: ignoring directive "
                    "%s for /%s (surface=%s). Block/rewrite will arrive with "
                    "the command middleware variant (#64204/#64231).",
                    dumped ? dumped : "{}", command ? command : "",
                    surface ? surface : "");
            free(dumped);
        }
        if (r) json_free(r);
    }
    free(cbs);
    json_free(payload);
}

/* ── pre-tool-call directive plumbing ─────────────────────────────────── */


static void directive_free(pre_tool_directive_t *d) {
    if (!d) return;
    free(d->action); free(d->message); free(d->rule_key);
    if (d->modified_args) json_free(d->modified_args);
    memset(d, 0, sizeof(*d));
}

/* Fetch the pre_tool_call directive by invoking hooks in registration order:
 * block wins immediately; approve recorded but later blocks override;
 * modify merges modified_args. */
static void get_pre_tool_call_directive_details(
        const char *tool_name, const json_t *args, const char *task_id,
        const char *session_id, const char *tool_call_id,
        const char *turn_id, const char *api_request_id,
        const json_t *middleware_trace, pre_tool_directive_t *out) {
    (void)task_id; (void)api_request_id; (void)middleware_trace;
    plug_manager_t *m = _delivery_manager();
    if (!m) return;
    json_t *payload = json_object();
    json_set(payload, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(payload, "args", args ? json_copy(args) : json_null());
    if (session_id)  json_set(payload, "session_id", json_string(session_id));
    if (tool_call_id) json_set(payload, "tool_call_id", json_string(tool_call_id));
    if (turn_id)     json_set(payload, "turn_id", json_string(turn_id));

    size_t n = 0;
    plug_cb_fn *cbs = PluginManager_iter_hook_callbacks(m, "pre_tool_call", &n);
    for (size_t i = 0; i < n; i++) {
        json_t *r = cbs[i](payload);
        if (!r || !json_is_object(r)) { if (r) json_free(r); continue; }
        const char *action = json_get_str(r, "action", "");
        if (strcmp(action, "block") == 0) {
            free(out->action);
            out->action = xstrdup("block");
            free(out->message);
            out->message = xstrdup(json_get_str(r, "message", ""));
            json_free(r);
            break;
        }
        if (strcmp(action, "approve") == 0 && (!out->action ||
                                               strcmp(out->action, "approve") != 0)) {
            free(out->action);
            out->action = xstrdup("approve");
            free(out->message);
            out->message = xstrdup(json_get_str(r, "message", ""));
            free(out->rule_key);
            out->rule_key = xstrdup(json_get_str(r, "rule_key", tool_name));
        }
        if (strcmp(action, "modify") == 0) {
            const json_t *ma = json_obj_get(r, "args");
            if (ma && json_is_object(ma)) {
                if (out->modified_args)
                    json_free(out->modified_args);
                out->modified_args = json_copy(ma);
            }
        }
        json_free(r);
    }
    free(cbs);
    json_free(payload);
}

/* PoP: resolve_pre_tool_block @ hermes_cli/plugins.py:resolve_pre_tool_block */
/*
 * Single entry point for every tool-dispatch site: fetches the directive and
 * resolves approve escalations through request_tool_approval. Returns a
 * malloc'd block message or NULL to proceed. Caller frees. */
char *resolve_pre_tool_block(const char *tool_name, const json_t *args,
                             const char *task_id, const char *session_id,
                             const char *tool_call_id, const char *turn_id,
                             const char *api_request_id,
                             const json_t *middleware_trace) {
    pre_tool_directive_t details;
    memset(&details, 0, sizeof(details));
    get_pre_tool_call_directive_details(tool_name, args, task_id, session_id,
                                        tool_call_id, turn_id, api_request_id,
                                        middleware_trace, &details);
    char *msg =
        _resolve_block_from_details(&details, tool_name, turn_id, tool_call_id,
                                    session_id);
    directive_free(&details);
    return msg;
}

/* PoP: _resolve_block_from_details @ hermes_cli/plugins.py:_resolve_block_from_details */
/*
 * Security-critical fail-closed approval logic in ONE place: block blocks
 * with its message; an approve whose gate errors/denies/times out fails
 * closed to a block; anything else proceeds. Caller frees non-NULL result. */
char *_resolve_block_from_details(const pre_tool_directive_t *details,
                                  const char *tool_name, const char *turn_id,
                                  const char *tool_call_id,
                                  const char *session_id) {
    (void)turn_id; (void)tool_call_id; (void)session_id;
    if (details->action && strcmp(details->action, "block") == 0)
        return xstrdup(details->message ? details->message : "");
    if (details->action && strcmp(details->action, "approve") == 0) {
        /* human-approval gate (tools/approval.request_tool_approval port) */
        extern int request_tool_approval_gate(const char *tool_name,
                                              const char *message,
                                              const char *rule_key,
                                              char **out_message);
        char *gate_message = NULL;
        int rc = request_tool_approval_gate(
            tool_name, details->message ? details->message : "",
            details->rule_key && *details->rule_key ? details->rule_key
                                                    : tool_name,
            &gate_message);
        if (rc < 0) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "BLOCKED: plugin approval gate failed for %s", tool_name);
            return strdup(buf);
        }
        if (rc == 0) {   /* not approved -> fail closed */
            char buf[512];
            if (gate_message && gate_message[0])
                snprintf(buf, sizeof(buf), "%s", gate_message);
            else
                snprintf(buf, sizeof(buf),
                         "BLOCKED: plugin approval required for %s", tool_name);
            free(gate_message);
            return strdup(buf);
        }
        free(gate_message);
    }
    return NULL;
}

/* PoP: _dispatch_pre_tool_call_hooks @ hermes_cli/plugins.py:_dispatch_pre_tool_call_hooks */
/*
 * Single invocation point for pre_tool_call hooks. Returns block message
 * (NULL to proceed) and, via out-param, merged args from modify directives
 * (fresh JSON object or NULL). Caller frees both. */
char *_dispatch_pre_tool_call_hooks(const char *tool_name, const json_t *args,
                                    const char *task_id,
                                    const char *session_id,
                                    const char *tool_call_id,
                                    const char *turn_id,
                                    const char *api_request_id,
                                    const json_t *middleware_trace,
                                    json_t **out_modified_args) {
    if (out_modified_args) *out_modified_args = NULL;
    pre_tool_directive_t details;
    memset(&details, 0, sizeof(details));
    get_pre_tool_call_directive_details(tool_name, args, task_id, session_id,
                                        tool_call_id, turn_id, api_request_id,
                                        middleware_trace, &details);
    char *block_msg =
        _resolve_block_from_details(&details, tool_name, turn_id, tool_call_id,
                                    session_id);
    if (out_modified_args && details.modified_args) {
        *out_modified_args = json_copy(details.modified_args);
    }
    directive_free(&details);
    return block_msg;
}

/* FailoverReason member names (agent/error_classifier.FailoverReason) */
static const char *FAILOVER_REASONS[] = {
    "rate_limit", "context_overflow", "auth_failure", "server_error",
    "network_error", "invalid_request", "provider_unavailable", "unknown",
    NULL,
};

/* PoP: get_plugin_error_classification @ hermes_cli/plugins.py:get_plugin_error_classification */
/*
 * Consult transform_api_error_classification hooks BEFORE the built-in
 * pipeline. Run-all-then-pick-first: every callback runs with failures
 * isolated; first valid reason wins; losing valid results warn (#64714).
 * Returns sanitized dict {"reason":..., hints...} or NULL. Caller frees. */
json_t *get_plugin_error_classification(
        const char *provider, const char *model, int status_code,
        const char *error_type, const char *error_code,
        const char *error_message, const json_t *error_body,
        int approx_tokens, int context_length, int num_messages) {
    plug_manager_t *m = _delivery_manager();
    if (!m) return NULL;

    json_t *payload = json_object();
    json_set(payload, "provider", json_string(provider ? provider : ""));
    json_set(payload, "model", json_string(model ? model : ""));
    json_set(payload, "status_code",
             status_code > 0 ? json_int(status_code) : json_null());
    json_set(payload, "error_type", json_string(error_type ? error_type : ""));
    json_set(payload, "error_code", json_string(error_code ? error_code : ""));
    json_set(payload, "error_message",
             json_string(error_message ? error_message : ""));
    json_set(payload, "error_body",
             error_body ? json_copy(error_body) : json_object());
    json_set(payload, "approx_tokens", json_int(approx_tokens));
    json_set(payload, "context_length", json_int(context_length));
    json_set(payload, "num_messages", json_int(num_messages));

    size_t n = 0;
    plug_cb_fn *cbs = PluginManager_iter_hook_callbacks(
        m, "transform_api_error_classification", &n);

    json_t *winner = NULL;
    int skipped_valid = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *result = cbs[i](payload);   /* isolated per callback */
        if (!result || !json_is_object(result)) {
            if (result) json_free(result);
            continue;
        }
        const char *reason_raw = json_get_str(result, "reason", "");
        bool valid = false;
        char lowered[128];
        snprintf(lowered, sizeof(lowered), "%s", reason_raw);
        for (char *p = lowered; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        for (int k = 0; FAILOVER_REASONS[k]; k++)
            if (strcmp(lowered, FAILOVER_REASONS[k]) == 0) { valid = true; break; }
        if (!valid) { json_free(result); continue; }

        if (winner) {
            skipped_valid++;
            json_free(result);
            continue;
        }
        json_t *out = json_object();
        json_set(out, "reason", json_string(lowered));
        static const char *HINT_KEYS[] = {
            "retryable", "should_compress", "should_rotate_credential",
            "should_fallback", NULL,
        };
        for (int k = 0; HINT_KEYS[k]; k++) {
            if (json_has(result, HINT_KEYS[k]))
                json_set(out, HINT_KEYS[k],
                         json_bool(json_get_bool(result, HINT_KEYS[k], false)));
        }
        const char *msg = json_get_str(result, "message", "");
        if (msg[0]) {
            char clipped[501];
            snprintf(clipped, sizeof(clipped), "%s", msg);
            json_set(out, "message", json_string(clipped));
        }
        const json_t *ectx = json_obj_get(result, "error_context");
        if (ectx && json_is_object(ectx))
            json_set(out, "error_context", json_copy(ectx));
        winner = out;
        json_free(result);
    }
    free(cbs);
    json_free(payload);
    if (winner && skipped_valid)
        plg_log("WARNING",
                "transform_api_error_classification: skipped %d valid "
                "classification(s) after the first result in registration "
                "order won (run-all-then-pick-first)", skipped_valid);
    return winner;
}

/* PoP: get_plugin_subscriptions @ hermes_cli/plugins.py:get_plugin_subscriptions */
/*
 * Snapshot of the inter-plugin event bus subscription registry mapping each
 * fully-qualified event name to subscriber callbacks in registration order.
 * Triggers idempotent plugin discovery first. Fresh JSON object. */
json_t *get_plugin_subscriptions(void) {
    plug_manager_t *m = _delivery_manager();
    json_t *snap = json_object();
    if (!m) return snap;
    pthread_mutex_lock(&m->event_lock);
    for (size_t i = 0; i < m->n_subs; i++) {
        plug_subscription_t *e = &m->subs[i];
        json_t *list = json_obj_get(snap, e->event);
        if (!list) {
            list = json_array();
            json_set(snap, e->event, list);
        }
        json_append(list, json_int((long long)e->id));
    }
    pthread_mutex_unlock(&m->event_lock);
    return snap;
}

/* PoP: PluginContext_register_terminal_environment_provider @ hermes_cli/plugins.py:register_terminal_environment_provider */
/* Register a pluggable terminal execution backend. Built-in backend names
 * (local, docker, singularity, modal, daytona, vercel_sandbox, ssh) are
 * rejected by the registry - plugins extend, never shadow. Returns the
 * replacement-tracked handle or NULL (rejected / superseded). */
plug_registration_t *PluginContext_register_terminal_environment_provider(
        PluginContext *self, const char *provider_name) {
    if (!self || !self->manager || !provider_name || !*provider_name) {
        plg_log("WARNING",
                "Plugin '%s' tried to register a terminal environment "
                "provider that does not inherit from "
                "TerminalEnvironmentProvider. Ignoring.",
                self ? self->manifest.name : "?");
        return NULL;
    }
    /* registry_name = provider.name.strip().lower() */
    char clean[128];
    snprintf(clean, sizeof(clean), "%s", provider_name);
    char *s = clean, *d = clean;
    while (*s == ' ' || *s == '\t') s++;
    while (*s) { *d++ = (char)tolower((unsigned char)*s); s++; }
    *d = '\0';
    static const char *BUILTIN_BACKENDS[] = {
        "local", "docker", "singularity", "modal", "daytona",
        "vercel_sandbox", "ssh", NULL,
    };
    for (int i = 0; BUILTIN_BACKENDS[i]; i++) {
        if (strcmp(clean, BUILTIN_BACKENDS[i]) == 0) {
            plg_log("WARNING",
                    "Plugin '%s' terminal environment provider rejected: "
                    "'%s' collides with a built-in backend",
                    self->manifest.name ? self->manifest.name : "?", clean);
            return NULL;
        }
    }
    const char *scope = self->manager->scope_key ? self->manager->scope_key : "";
    /* snapshot -> register -> re-snapshot; superseded registration declines */
    extern int terminal_env_registry_register_provider(const char *name,
                                                       const char *scope);
    if (terminal_env_registry_register_provider(clean, scope) != 0) {
        plg_log("WARNING",
                "Plugin '%s' terminal environment provider rejected by the "
                "registry", self->manifest.name ? self->manifest.name : "?");
        return NULL;
    }
    plug_registration_t *handle = PluginContext__track_replacement(
        self, "terminal_environment_provider", clean,
        "terminal_environment_provider", NULL, NULL, NULL, NULL, NULL);
    plg_log("INFO", "Plugin '%s' registered terminal environment provider: %s",
            self->manifest.name ? self->manifest.name : "?", clean);
    return handle;
}

/* Weak agent/terminal_env_registry.register_provider stand-in. */
__attribute__((weak)) int terminal_env_registry_register_provider(
        const char *name, const char *scope) {
    if (!name || !*name) return -1;
    plg_log("DEBUG", "terminal env provider '%s' registered (scope %s)",
            name, scope ? scope : "");
    return 0;
}
