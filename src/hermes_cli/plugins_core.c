/* plugins_core.c — faithful C11 port of the pure-logic surface of
 * hermes_cli/plugins.py.
 *
 * Reuses: hermes_json.h (libjson + the libyaml bridge for plugin.yaml),
 * lib/libcrypto (crypto_sha256). Nothing else — self-contained module.
 */
#define _GNU_SOURCE
#include "plugins_core.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#include "hermes_json.h"
#include "../../lib/libcrypto/crypto.h"

/* ── module constants (mirror the Python module-level names) ───────────── */

/* PoP: plugins_sections_start_marker @ hermes_cli/plugins.py:PLUGIN_SECTIONS_START */
static const char *const SECTIONS_START = "<!-- hermes-plugin-sections:start -->";
/* PoP: plugins_sections_end_marker @ hermes_cli/plugins.py:PLUGIN_SECTIONS_END */
static const char *const SECTIONS_END = "<!-- hermes-plugin-sections:end -->";
/* _SYSTEM_PROMPT_SECTION_HEADING_PREFIX */
static const char *const SECTION_HEADING_PREFIX = "## Plugin Context: ";

/* _VALID_PLUGIN_KINDS */
static const char *const VALID_PLUGIN_KINDS[] = {
    "standalone", "backend", "exclusive", "platform", "model-provider", NULL
};

const char *plugins_sections_start_marker(void) { return SECTIONS_START; }
const char *plugins_sections_end_marker(void)   { return SECTIONS_END; }

/* ── local utilities ──────────────────────────────────────────────────── */

void plugins_free_list(char **list, size_t n) {
    if (!list) return;
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}

static char *pc_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *plg_dep_vr_str(const json_t *vr);

/* Count Unicode codepoints in a UTF-8 string (Python's len() on a str).
 * strlen counts bytes; Python's len(content) counts codepoints. */
static size_t pc_utf8_codepoints(const char *s) {
    if (!s) return 0;
    size_t n = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) s += 1;
        else if ((c & 0xE0) == 0xC0) s += 2;
        else if ((c & 0xF0) == 0xE0) s += 3;
        else if ((c & 0xF8) == 0xF0) s += 4;
        else s += 1; /* invalid leading byte — skip one */
        n++;
    }
    return n;
}

static char *pc_join(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0;
    char *out = (char *)malloc(la + lb + 2);
    if (!out) return NULL;
    memcpy(out, a ? a : "", la);
    size_t k = la;
    if (k && out[k - 1] != '/') out[k++] = '/';
    memcpy(out + k, b ? b : "", lb);
    out[k + lb] = '\0';
    return out;
}

static bool pc_is_dir(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool pc_exists(const char *path) {
    struct stat st;
    return path && (stat(path, &st) == 0 || lstat(path, &st) == 0);
}

static char *pc_read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* hermes_constants.py:env_var_enabled — truthy opt-in. */
static bool pc_env_enabled(const char *name) {
    const char *v = getenv(name);
    if (!v) return false;
    while (*v == ' ' || *v == '\t') v++;
    return strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 ||
           strcasecmp(v, "yes") == 0 || strcasecmp(v, "on") == 0;
}

/* hermes_constants.get_hermes_home() */
static char *pc_hermes_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return pc_strdup(h);
    const char *home = getenv("HOME");
    return pc_join(home ? home : "", ".hermes");
}

/* PoP: plugins_collect_directory_manifests @ hermes_cli/plugins.py:get_bundled_plugins_dir */
static char *pc_bundled_plugins_dir(void) {
    const char *env = getenv("HERMES_BUNDLED_PLUGINS");
    if (env && *env) return pc_strdup(env);
    /* Path(__file__).resolve().parent.parent / "plugins" — the repo root the
     * running binary was built from; HERMES_PROJECT_ROOT carries it in C. */
    const char *root = getenv("HERMES_PROJECT_ROOT");
    if (root && *root) return pc_join(root, "plugins");
    return pc_strdup("plugins");
}

/* ── system-prompt sections ───────────────────────────────────────────── */

/* PoP: plugins_is_valid_system_prompt_section_id @ hermes_cli/plugins.py:is_valid_system_prompt_section_id */
bool plugins_is_valid_system_prompt_section_id(const char *value) {
    if (!value || !*value) return false;
    /* ^[a-z0-9][a-z0-9._-]{0,127}$ */
    unsigned char c0 = (unsigned char)value[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) return false;
    size_t n = strlen(value);
    if (n - 1 > 127) return false;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

/* PoP: plugins_format_system_prompt_section @ hermes_cli/plugins.py:format_system_prompt_section */
char *plugins_format_system_prompt_section(const char *section_id,
                                           const char *content) {
    const char *id = section_id ? section_id : "";
    const char *body = content ? content : "";
    size_t clen = pc_utf8_codepoints(body);
    size_t need = strlen(SECTION_HEADING_PREFIX) + strlen(id) + 64 + strlen(body) + 8;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need,
             "%s%s\n<!-- hermes-plugin-section-chars:%zu -->\n\n%s",
             SECTION_HEADING_PREFIX, id, clen, body);
    return out;
}

/* PoP: plugins_format_system_prompt_sections @ hermes_cli/plugins.py:format_system_prompt_sections */
char *plugins_format_system_prompt_sections(const plugins_section_t *sections,
                                            size_t n_sections) {
    if (!sections || n_sections == 0) return pc_strdup("");
    char **blocks = (char **)calloc(n_sections, sizeof *blocks);
    if (!blocks) return NULL;
    size_t total = strlen(SECTIONS_START) + 1 + strlen(SECTIONS_END) + 2;
    for (size_t i = 0; i < n_sections; i++) {
        blocks[i] = plugins_format_system_prompt_section(sections[i].id,
                                                         sections[i].content);
        if (!blocks[i]) { plugins_free_list(blocks, n_sections); return NULL; }
        total += strlen(blocks[i]) + 2;
    }
    char *out = (char *)malloc(total + 1);
    if (!out) { plugins_free_list(blocks, n_sections); return NULL; }
    size_t k = 0;
    k += (size_t)sprintf(out + k, "%s\n", SECTIONS_START);
    for (size_t i = 0; i < n_sections; i++) {
        if (i) k += (size_t)sprintf(out + k, "\n\n");
        k += (size_t)sprintf(out + k, "%s", blocks[i]);
    }
    sprintf(out + k, "\n%s", SECTIONS_END);
    plugins_free_list(blocks, n_sections);
    return out;
}

/* ── kind detection ───────────────────────────────────────────────────── */

/* PoP: plugins_detect_kind_from_source @ hermes_cli/plugins.py:_detect_kind_from_source */
const char *plugins_detect_kind_from_source(const char *source_text) {
    if (!source_text) return NULL;
    if (strstr(source_text, "register_memory_provider") ||
        strstr(source_text, "MemoryProvider"))
        return "exclusive";
    if (strstr(source_text, "register_provider") &&
        strstr(source_text, "ProviderProfile"))
        return "model-provider";
    return NULL;
}

/* PoP: plugins_classify_entrypoint_value_kind @ hermes_cli/plugins.py:_classify_entrypoint_value_kind */
const char *plugins_classify_entrypoint_value_kind(
    const char *value,
    char *(*source_reader)(const char *module_name)) {
    if (!value || !source_reader) return "standalone";
    /* str(value).split(":", 1)[0].strip() */
    const char *colon = strchr(value, ':');
    size_t len = colon ? (size_t)(colon - value) : strlen(value);
    const char *s = value;
    while (len && (*s == ' ' || *s == '\t')) { s++; len--; }
    while (len && (s[len-1]==' '||s[len-1]=='\t')) len--;
    if (!len) return "standalone";
    char *module_name = (char *)malloc(len + 1);
    if (!module_name) return "standalone";
    memcpy(module_name, s, len);
    module_name[len] = '\0';
    char *source = source_reader(module_name);
    free(module_name);
    const char *kind = plugins_detect_kind_from_source(source ? source : "");
    free(source);
    return kind ? kind : "standalone";
}

/* plugins_portable_skill_namespace lives in plugins_wrappers.c (UTF-8
 * codepoint-aware, matching Python's per-character iteration). Declared in
 * plugins_core.h; reused here rather than duplicated. */

/* ── load order (graphlib.TopologicalSorter parity) ───────────────────── */

/* PoP: plugins_resolve_plugin_load_order @ hermes_cli/plugins.py:resolve_plugin_load_order */
const char **plugins_resolve_plugin_load_order(const plugins_dep_node_t *nodes,
                                               size_t n_nodes,
                                               size_t *out_n) {
    if (out_n) *out_n = 0;
    if (!nodes || n_nodes == 0) return NULL;

    /* keys = sorted(manifests.keys()) */
    size_t *order = (size_t *)calloc(n_nodes, sizeof *order);
    if (!order) return NULL;
    for (size_t i = 0; i < n_nodes; i++) order[i] = i;
    for (size_t i = 1; i < n_nodes; i++) {
        size_t v = order[i];
        size_t j = i;
        while (j > 0 && strcmp(nodes[order[j-1]].key ? nodes[order[j-1]].key : "",
                               nodes[v].key ? nodes[v].key : "") > 0) {
            order[j] = order[j-1];
            j--;
        }
        order[j] = v;
    }

    /* by_name: first key claiming each display name (in sorted key order). */
    /* edges[k] = resolved dependency indices */
    bool *edge = (bool *)calloc(n_nodes * n_nodes, sizeof *edge);
    if (!edge) { free(order); return NULL; }

    for (size_t oi = 0; oi < n_nodes; oi++) {
        size_t i = order[oi];
        const char *const *reqs = nodes[i].requires;
        if (!reqs) continue;
        for (; *reqs; reqs++) {
            const char *dep_id = *reqs;
            if (!dep_id || !*dep_id) continue;
            /* _resolve_dep: exact key first, then by display name. */
            long resolved = -1;
            for (size_t oj = 0; oj < n_nodes && resolved < 0; oj++) {
                size_t j = order[oj];
                if (nodes[j].key && strcmp(nodes[j].key, dep_id) == 0)
                    resolved = (long)j;
            }
            if (resolved < 0) {
                for (size_t oj = 0; oj < n_nodes && resolved < 0; oj++) {
                    size_t j = order[oj];
                    if (nodes[j].name && *nodes[j].name &&
                        strcmp(nodes[j].name, dep_id) == 0)
                        resolved = (long)j;
                }
            }
            if (resolved < 0) continue;                 /* missing → warn+skip */
            if ((size_t)resolved == i) continue;        /* self → ignore */
            edge[i * n_nodes + (size_t)resolved] = true;
        }
    }

    /* Kahn with ties broken alphabetically (get_ready() sorted). */
    size_t *indeg = (size_t *)calloc(n_nodes, sizeof *indeg);
    bool *done = (bool *)calloc(n_nodes, sizeof *done);
    const char **out = (const char **)calloc(n_nodes, sizeof *out);
    if (!indeg || !done || !out) {
        free(order); free(edge); free(indeg); free(done); free(out);
        return NULL;
    }
    for (size_t i = 0; i < n_nodes; i++)
        for (size_t j = 0; j < n_nodes; j++)
            if (edge[i * n_nodes + j]) indeg[i]++;

    size_t emitted = 0;
    while (emitted < n_nodes) {
        size_t batch = 0;
        /* iterate in sorted-key order so ready batches are alphabetical */
        for (size_t oi = 0; oi < n_nodes; oi++) {
            size_t i = order[oi];
            if (done[i] || indeg[i] != 0) continue;
            out[emitted + batch] = nodes[i].key;
            batch++;
        }
        if (batch == 0) {
            /* graphlib.CycleError → alphabetical order for ALL plugins. */
            for (size_t oi = 0; oi < n_nodes; oi++)
                out[oi] = nodes[order[oi]].key;
            emitted = n_nodes;
            break;
        }
        /* sorter.done(*ready) */
        for (size_t b = 0; b < batch; b++) {
            const char *key = out[emitted + b];
            for (size_t i = 0; i < n_nodes; i++) {
                if (done[i] || nodes[i].key != key) continue;
                done[i] = true;
                for (size_t k = 0; k < n_nodes; k++)
                    if (edge[k * n_nodes + i] && indeg[k]) indeg[k]--;
                break;
            }
        }
        emitted += batch;
    }

    free(order); free(edge); free(indeg); free(done);
    if (out_n) *out_n = n_nodes;
    return out;
}

/* ── directory manifest scanning ──────────────────────────────────────── */

typedef struct {
    char *key, *name, *path, *source, *kind, *skill_namespace;
    bool portable;
} pc_manifest_t;

struct plugins_scan_result_t {
    pc_manifest_t *items;
    size_t count, cap;
};

static plugins_scan_result_t *pc_result_new(void) {
    return (plugins_scan_result_t *)calloc(1, sizeof(plugins_scan_result_t));
}

static void pc_result_push(plugins_scan_result_t *r, pc_manifest_t m) {
    if (r->count == r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 8;
        pc_manifest_t *ni = (pc_manifest_t *)realloc(r->items, nc * sizeof *ni);
        if (!ni) return;
        r->items = ni;
        r->cap = nc;
    }
    r->items[r->count++] = m;
}

void plugins_scan_free(plugins_scan_result_t *r) {
    if (!r) return;
    for (size_t i = 0; i < r->count; i++) {
        free(r->items[i].key);
        free(r->items[i].name);
        free(r->items[i].path);
        free(r->items[i].source);
        free(r->items[i].kind);
        free(r->items[i].skill_namespace);
    }
    free(r->items);
    free(r);
}

size_t plugins_scan_count(const plugins_scan_result_t *r) { return r ? r->count : 0; }
#define PC_ACC(fn, field) \
    const char *fn(const plugins_scan_result_t *r, size_t i) { \
        return (r && i < r->count) ? r->items[i].field : NULL; }
PC_ACC(plugins_scan_key, key)
PC_ACC(plugins_scan_name, name)
PC_ACC(plugins_scan_path, path)
PC_ACC(plugins_scan_source, source)
PC_ACC(plugins_scan_kind, kind)
PC_ACC(plugins_scan_skill_namespace, skill_namespace)
#undef PC_ACC
bool plugins_scan_portable(const plugins_scan_result_t *r, size_t i) {
    return (r && i < r->count) ? r->items[i].portable : false;
}

/* PoP: plugins_scan_directory @ hermes_cli/plugins.py:PluginManager._parse_manifest */
static bool pc_parse_manifest(const char *manifest_file, const char *plugin_dir,
                             const char *dir_name, const char *source,
                             const char *prefix, pc_manifest_t *out) {
    char *text = pc_read_text(manifest_file);
    if (!text) return false;
    json_t *data = json_parse_yaml_real(text);
    free(text);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return false;
    }

    const char *name = json_get_str(data, "name", dir_name);
    char *key = prefix && *prefix ? pc_join(prefix, dir_name) : pc_strdup(name);

    /* kind = str(data.get("kind","standalone")).strip().lower() */
    json_t *raw = json_obj_get(data, "kind");
    bool kind_declared = raw != NULL;
    const char *raw_kind = (raw && raw->type == JSON_STRING && raw->str_val)
                           ? raw->str_val : "standalone";
    char kind[64];
    {
        const char *s = raw_kind;
        while (*s == ' ' || *s == '\t') s++;
        size_t n = strlen(s);
        while (n && (s[n-1] == ' ' || s[n-1] == '\t')) n--;
        if (n >= sizeof kind) n = sizeof kind - 1;
        for (size_t i = 0; i < n; i++) {
            char c = s[i];
            kind[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        kind[n] = '\0';
    }
    bool valid = false;
    for (const char *const *k = VALID_PLUGIN_KINDS; *k; k++)
        if (strcmp(kind, *k) == 0) { valid = true; break; }
    if (!valid) snprintf(kind, sizeof kind, "standalone");

    /* Auto-coerce undeclared standalone kinds from __init__.py markers. */
    if (strcmp(kind, "standalone") == 0 && !kind_declared) {
        char *init_file = pc_join(plugin_dir, "__init__.py");
        if (init_file && pc_exists(init_file)) {
            char *src = pc_read_text(init_file);
            if (src) {
                if (strlen(src) > 8192) src[8192] = '\0';  /* [:8192] */
                const char *detected = plugins_detect_kind_from_source(src);
                if (detected) snprintf(kind, sizeof kind, "%s", detected);
                free(src);
            }
        }
        free(init_file);
    }

    out->key = key;
    out->name = pc_strdup(name);
    out->path = pc_strdup(plugin_dir);
    out->source = pc_strdup(source);
    out->kind = pc_strdup(kind);
    out->skill_namespace = NULL;
    out->portable = false;
    json_free(data);
    return true;
}

/* PoP: plugins_scan_directory @ hermes_cli/plugins.py:PluginManager._scan_directory_level */
static void pc_scan_level(plugins_scan_result_t *r, const char *path,
                          const char *source, const char *const *skip_names,
                          const char *prefix, int depth) {
    if (!pc_is_dir(path)) return;
    DIR *d = opendir(path);
    if (!d) return;

    /* sorted(path.iterdir()) */
    char **children = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (n == cap) {
            size_t nc = cap ? cap * 2 : 16;
            char **nb = (char **)realloc(children, nc * sizeof *nb);
            if (!nb) break;
            children = nb;
            cap = nc;
        }
        children[n++] = pc_strdup(e->d_name);
    }
    closedir(d);
    for (size_t i = 1; i < n; i++) {
        char *v = children[i];
        size_t j = i;
        while (j > 0 && strcmp(children[j-1], v) > 0) { children[j] = children[j-1]; j--; }
        children[j] = v;
    }

    for (size_t i = 0; i < n; i++) {
        char *child = pc_join(path, children[i]);
        if (!child || !pc_is_dir(child)) { free(child); continue; }
        if (depth == 0 && skip_names) {
            bool skip = false;
            for (const char *const *s = skip_names; *s && !skip; s++)
                if (strcmp(children[i], *s) == 0) skip = true;
            if (skip) { free(child); continue; }
        }

        char *manifest_file = pc_join(child, "plugin.yaml");
        if (manifest_file && !pc_exists(manifest_file)) {
            free(manifest_file);
            manifest_file = pc_join(child, "plugin.yml");
        }
        if (manifest_file && pc_exists(manifest_file)) {
            pc_manifest_t m;
            memset(&m, 0, sizeof m);
            if (pc_parse_manifest(manifest_file, child, children[i], source,
                                  prefix, &m))
                pc_result_push(r, m);
            free(manifest_file);
            free(child);
            continue;
        }
        free(manifest_file);

        char *portable_file = pc_join(child, "plugin.json");
        if (portable_file && pc_exists(portable_file)) {
            char *text = pc_read_text(portable_file);
            json_t *data = text ? json_parse(text, NULL) : NULL;
            free(text);
            if (data && data->type == JSON_OBJECT) {
                const char *pname = json_get_str(data, "name", children[i]);
                pc_manifest_t m;
                memset(&m, 0, sizeof m);
                m.key = (prefix && *prefix) ? pc_join(prefix, children[i])
                                            : pc_strdup(pname);
                m.name = pc_strdup(pname);
                m.path = pc_strdup(child);
                m.source = pc_strdup(source);
                m.kind = pc_strdup("standalone");
                m.portable = true;
                m.skill_namespace = plugins_portable_skill_namespace(m.key);
                pc_result_push(r, m);
            }
            if (data) json_free(data);
            free(portable_file);
            free(child);
            continue;
        }
        free(portable_file);

        /* No manifest: category namespace, capped at depth 2. */
        if (depth >= 1) { free(child); continue; }
        char *sub_prefix = (prefix && *prefix) ? pc_join(prefix, children[i])
                                              : pc_strdup(children[i]);
        pc_scan_level(r, child, source, NULL, sub_prefix, depth + 1);
        free(sub_prefix);
        free(child);
    }
    plugins_free_list(children, n);
}

/* PoP: plugins_scan_directory @ hermes_cli/plugins.py:PluginManager._scan_directory */
plugins_scan_result_t *plugins_scan_directory(const char *path,
                                              const char *source,
                                              const char *const *skip_names) {
    plugins_scan_result_t *r = pc_result_new();
    if (!r) return NULL;
    pc_scan_level(r, path, source ? source : "", skip_names, "", 0);
    return r;
}

static void pc_result_extend(plugins_scan_result_t *dst,
                             plugins_scan_result_t *src) {
    if (!dst || !src) return;
    for (size_t i = 0; i < src->count; i++) pc_result_push(dst, src->items[i]);
    free(src->items);          /* ownership of the strings moved to dst */
    free(src);
}

/* PoP: plugins_collect_directory_manifests @ hermes_cli/plugins.py:PluginManager._collect_directory_manifests */
plugins_scan_result_t *plugins_collect_directory_manifests(void) {
    plugins_scan_result_t *out = pc_result_new();
    if (!out) return NULL;

    /* 1. bundled, minus the four self-discovering categories. */
    static const char *const SKIP[] = { "memory", "context_engine",
                                        "platforms", "model-providers", NULL };
    char *repo_plugins = pc_bundled_plugins_dir();
    pc_result_extend(out, plugins_scan_directory(repo_plugins, "bundled", SKIP));
    char *bundled_platforms = pc_join(repo_plugins, "platforms");
    pc_result_extend(out, plugins_scan_directory(bundled_platforms, "bundled", NULL));
    free(bundled_platforms);
    free(repo_plugins);

    /* 2. user plugins (~/.hermes/plugins). */
    char *home = pc_hermes_home();
    char *user_dir = pc_join(home, "plugins");
    pc_result_extend(out, plugins_scan_directory(user_dir, "user", NULL));
    free(user_dir);
    free(home);

    /* 3. project plugins, opt-in only. */
    if (pc_env_enabled("HERMES_ENABLE_PROJECT_PLUGINS")) {
        char cwd[4096];
        if (getcwd(cwd, sizeof cwd)) {
            char *hermes_dir = pc_join(cwd, ".hermes");
            char *project_dir = pc_join(hermes_dir, "plugins");
            pc_result_extend(out,
                plugins_scan_directory(project_dir, "project", NULL));
            free(project_dir);
            free(hermes_dir);
        }
    }
    return out;
}

/* PoP: plugins_has_enabled_portable_mcp @ hermes_cli/plugins.py:PluginManager.has_enabled_portable_mcp */
bool plugins_has_enabled_portable_mcp(const char *config_json) {
    if (pc_env_enabled("HERMES_SAFE_MODE")) return false;
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    if (!cfg || cfg->type != JSON_OBJECT) { if (cfg) json_free(cfg); return false; }
    json_t *plugins_cfg = json_obj_get(cfg, "plugins");
    if (!plugins_cfg || plugins_cfg->type != JSON_OBJECT) {
        json_free(cfg);
        return false;
    }
    json_t *enabled = json_obj_get(plugins_cfg, "enabled");
    if (!enabled || enabled->type != JSON_ARRAY) { json_free(cfg); return false; }
    json_t *disabled = json_obj_get(plugins_cfg, "disabled");
    if (disabled && disabled->type != JSON_ARRAY) disabled = NULL;
    if (json_len(enabled) == 0) { json_free(cfg); return false; }

    plugins_scan_result_t *manifests = plugins_collect_directory_manifests();
    bool found = false;

    /* winners: last manifest per key wins (dict assignment order). */
    for (size_t i = 0; i < plugins_scan_count(manifests) && !found; i++) {
        const char *key = plugins_scan_key(manifests, i);
        if (!key) continue;
        bool superseded = false;
        for (size_t j = i + 1; j < plugins_scan_count(manifests); j++) {
            const char *k2 = plugins_scan_key(manifests, j);
            if (k2 && strcmp(k2, key) == 0) { superseded = true; break; }
        }
        if (superseded) continue;
        if (!plugins_scan_portable(manifests, i)) continue;

        const char *name = plugins_scan_name(manifests, i);
        bool is_disabled = false, is_enabled = false;
        if (disabled) {
            for (size_t j = 0; j < json_len(disabled); j++) {
                json_t *v = json_get(disabled, j);
                if (!v || v->type != JSON_STRING || !v->str_val) continue;
                if (strcmp(v->str_val, key) == 0 ||
                    (name && strcmp(v->str_val, name) == 0)) is_disabled = true;
            }
        }
        if (is_disabled) continue;
        for (size_t j = 0; j < json_len(enabled); j++) {
            json_t *v = json_get(enabled, j);
            if (!v || v->type != JSON_STRING || !v->str_val) continue;
            if (strcmp(v->str_val, key) == 0 ||
                (name && strcmp(v->str_val, name) == 0)) is_enabled = true;
        }
        if (!is_enabled) continue;

        /* _discover_mcp(path, ..., create_data=False): a readable mcp.json
         * declaring at least one server. */
        const char *path = plugins_scan_path(manifests, i);
        char *mcp_path = pc_join(path, "mcp.json");
        char *text = mcp_path ? pc_read_text(mcp_path) : NULL;
        if (text) {
            json_t *mcp = json_parse(text, NULL);
            if (mcp && mcp->type == JSON_OBJECT) {
                json_t *servers = json_obj_get(mcp, "mcpServers");
                if (!servers) servers = json_obj_get(mcp, "mcp_servers");
                if (servers && servers->type == JSON_OBJECT &&
                    json_object_size(servers) > 0)
                    found = true;
                else if (json_object_size(mcp) > 0 && !servers)
                    found = true;   /* flat {name: config} form */
            }
            if (mcp) json_free(mcp);
            free(text);
        }
        free(mcp_path);
    }

    plugins_scan_free(manifests);
    json_free(cfg);
    return found;
}

/* ── toolset-key cache ────────────────────────────────────────────────── */

/* PoP: plugins_toolset_keys_cache_path @ hermes_cli/plugins.py:_plugin_toolset_keys_cache_path */
char *plugins_toolset_keys_cache_path(void) {
    char *home = pc_hermes_home();
    if (!home) return NULL;
    char *cache = pc_join(home, "cache");
    free(home);
    if (!cache) return NULL;
    char *out = pc_join(cache, "plugin_toolset_keys.json");
    free(cache);
    return out;
}

static json_t *pc_sorted_unique_array(const char *const *items, size_t n) {
    json_t *arr = json_array();
    if (!items || n == 0) return arr;
    size_t *order = (size_t *)calloc(n, sizeof *order);
    if (!order) return arr;
    for (size_t i = 0; i < n; i++) order[i] = i;
    for (size_t i = 1; i < n; i++) {
        size_t v = order[i];
        size_t j = i;
        while (j > 0 && strcmp(items[order[j-1]] ? items[order[j-1]] : "",
                               items[v] ? items[v] : "") > 0) {
            order[j] = order[j-1]; j--;
        }
        order[j] = v;
    }
    const char *prev = NULL;
    for (size_t i = 0; i < n; i++) {
        const char *s = items[order[i]];
        if (!s) continue;
        if (prev && strcmp(prev, s) == 0) continue;   /* set() dedupe */
        json_append(arr, json_string(s));
        prev = s;
    }
    free(order);
    return arr;
}

/* PoP: plugins_persist_toolset_keys @ hermes_cli/plugins.py:_persist_plugin_toolset_keys */
void plugins_persist_toolset_keys(const char *const *toolset_keys,
                                  size_t n_toolset_keys,
                                  const char *const *portable_mcp,
                                  size_t n_portable_mcp) {
    char *path = plugins_toolset_keys_cache_path();
    if (!path) return;

    /* path.parent.mkdir(parents=True, exist_ok=True) */
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = '\0';
        for (char *p = path + 1; *p; p++) {
            if (*p != '/') continue;
            *p = '\0';
            mkdir(path, 0700);
            *p = '/';
        }
        mkdir(path, 0700);
        *slash = '/';
    }

    json_t *blob = json_object();
    json_set(blob, "toolset_keys",
             pc_sorted_unique_array(toolset_keys, n_toolset_keys));
    json_set(blob, "portable_mcp",
             pc_sorted_unique_array(portable_mcp, n_portable_mcp));
    char *text = json_serialize(blob);
    json_free(blob);
    if (!text) { free(path); return; }

    /* mkstemp in the same dir, write, os.replace → atomic. */
    size_t dirlen = slash ? (size_t)(slash - path) : 1;
    char *tmp = (char *)malloc(dirlen + 32);
    if (tmp) {
        memcpy(tmp, path, dirlen);
        strcpy(tmp + dirlen, "/.pt_keys.XXXXXX");
        int fd = mkstemp(tmp);
        if (fd >= 0) {
            size_t len = strlen(text);
            bool ok = write(fd, text, len) == (ssize_t)len;
            close(fd);
            if (ok) {
                if (rename(tmp, path) != 0) unlink(tmp);
            } else {
                unlink(tmp);
            }
        }
        free(tmp);
    }
    free(text);
    free(path);
}

/* PoP: plugins_read_keys_cache @ hermes_cli/plugins.py:_read_plugin_keys_cache */
char *plugins_read_keys_cache(void) {
    char *path = plugins_toolset_keys_cache_path();
    if (!path) return NULL;
    char *text = pc_read_text(path);
    free(path);
    if (!text) return NULL;
    json_t *blob = json_parse(text, NULL);
    if (!blob || blob->type != JSON_OBJECT) {
        if (blob) json_free(blob);
        free(text);
        return NULL;         /* isinstance(blob, dict) guard */
    }
    json_free(blob);
    return text;
}

/* Immediate subdirectories of `dir` that ship a SKILL.md, sorted. Backs
 * list_plugin_skill_metadata's qualified-name enumeration. */
char **plugins_list_skill_dirs(const char *dir, size_t *out_n) {
    if (out_n) *out_n = 0;
    if (!pc_is_dir(dir)) return NULL;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char **out = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char *child = pc_join(dir, e->d_name);
        if (!child) continue;
        char *skill = pc_join(child, "SKILL.md");
        bool ok = pc_is_dir(child) && skill && pc_exists(skill);
        free(skill);
        free(child);
        if (!ok) continue;
        if (n == cap) {
            size_t nc = cap ? cap * 2 : 8;
            char **nb = (char **)realloc(out, nc * sizeof *nb);
            if (!nb) break;
            out = nb;
            cap = nc;
        }
        out[n++] = pc_strdup(e->d_name);
    }
    closedir(d);
    for (size_t i = 1; i < n; i++) {
        char *v = out[i];
        size_t j = i;
        while (j > 0 && strcmp(out[j-1], v) > 0) { out[j] = out[j-1]; j--; }
        out[j] = v;
    }
    if (out_n) *out_n = n;
    return out;
}

char **plugins_cached_string_list(const char *field, size_t *out_n) {
    if (out_n) *out_n = 0;
    char *text = plugins_read_keys_cache();
    if (!text) return NULL;
    json_t *blob = json_parse(text, NULL);
    free(text);
    if (!blob) return NULL;
    json_t *list = json_obj_get(blob, field ? field : "");
    if (!list || list->type != JSON_ARRAY) { json_free(blob); return NULL; }
    size_t n = json_len(list);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(list, i);
        if (!v || v->type != JSON_STRING) { json_free(blob); return NULL; }
    }
    char **out = (char **)calloc(n ? n : 1, sizeof *out);
    if (!out) { json_free(blob); return NULL; }
    for (size_t i = 0; i < n; i++)
        out[i] = pc_strdup(json_get(list, i)->str_val);
    json_free(blob);
    if (out_n) *out_n = n;
    return out;
}

/* === merged from plugins_gaps.c === */
/*
 * plugins_gaps.c — hermes_cli/plugins.py REAL_GAP surface.
 *
 * These entry points used to be hollow constant-returning stubs. They are now
 * thin delegates onto the real implementations in plugins_core.c (manifest
 * scanning, portable-MCP probing, the toolset-key cache) so every caller sees
 * the same behavior the Python module has.
 */

#include "plugins_core.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Serialize an array of borrowed strings into out_json. */
static void pg_emit_string_array(const char *const *items, size_t n,
                                 char *out_json, size_t out_sz) {
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        if (items[i]) json_append(arr, json_string(items[i]));
    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}

/* PoP: plugin_collect_directory_manifests @ hermes_cli/plugins.py:_collect_directory_manifests */
void plugin_collect_directory_manifests(char *out_json, size_t out_sz) {
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    json_t *arr = json_array();
    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        json_t *o = json_object();
        json_set(o, "key", json_string(plugins_scan_key(r, i) ?: ""));
        json_set(o, "name", json_string(plugins_scan_name(r, i) ?: ""));
        json_set(o, "path", json_string(plugins_scan_path(r, i) ?: ""));
        json_set(o, "source", json_string(plugins_scan_source(r, i) ?: ""));
        json_set(o, "kind", json_string(plugins_scan_kind(r, i) ?: ""));
        json_set(o, "portable", json_bool(plugins_scan_portable(r, i)));
        const char *ns = plugins_scan_skill_namespace(r, i);
        json_set(o, "skill_namespace", ns ? json_string(ns) : json_null());
        json_append(arr, o);
    }
    plugins_scan_free(r);
    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}

/*
 * PoP: plugin_has_enabled_portable_mcp @ hermes_cli/plugins.py:has_enabled_portable_mcp
 */
int plugin_has_enabled_portable_mcp(const char *raw_config_json) {
    return plugins_has_enabled_portable_mcp(raw_config_json) ? 1 : 0;
}

/*
 * Python: PluginManager().has_enabled_portable_mcp(raw_config) — a fresh
 * PoP: plugin_has_enabled_agent_plugin_mcp @ hermes_cli/plugins.py:has_enabled_agent_plugin_mcp
 */
int plugin_has_enabled_agent_plugin_mcp(const char *raw_config_json) {
    return plugins_has_enabled_portable_mcp(raw_config_json) ? 1 : 0;
}

/* Enabled portable packages' MCP server configs, keyed by server name.
 * Python keeps these in PluginManager._portable_mcp_servers, populated from
 * each enabled portable package's mcp.json during discovery; the C port
 * derives the same mapping from the manifests on demand. */
static json_t *pg_portable_mcp_servers(const char *raw_config_json) {
    json_t *out = json_object();
    json_t *cfg = raw_config_json ? json_parse(raw_config_json, NULL) : NULL;
    json_t *plugins_cfg = (cfg && cfg->type == JSON_OBJECT)
                          ? json_obj_get(cfg, "plugins") : NULL;
    json_t *enabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                      ? json_obj_get(plugins_cfg, "enabled") : NULL;
    json_t *disabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                       ? json_obj_get(plugins_cfg, "disabled") : NULL;
    if (!enabled || enabled->type != JSON_ARRAY) {
        if (cfg) json_free(cfg);
        return out;
    }
    if (disabled && disabled->type != JSON_ARRAY) disabled = NULL;

    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        if (!plugins_scan_portable(r, i)) continue;
        const char *key = plugins_scan_key(r, i);
        const char *name = plugins_scan_name(r, i);
        bool listed = false, blocked = false;
        for (size_t j = 0; j < json_len(enabled); j++) {
            json_t *v = json_get(enabled, j);
            if (v && v->type == JSON_STRING && v->str_val &&
                ((key && strcmp(v->str_val, key) == 0) ||
                 (name && strcmp(v->str_val, name) == 0))) listed = true;
        }
        if (disabled) {
            for (size_t j = 0; j < json_len(disabled); j++) {
                json_t *v = json_get(disabled, j);
                if (v && v->type == JSON_STRING && v->str_val &&
                    ((key && strcmp(v->str_val, key) == 0) ||
                     (name && strcmp(v->str_val, name) == 0))) blocked = true;
            }
        }
        if (!listed || blocked) continue;

        const char *path = plugins_scan_path(r, i);
        if (!path) continue;
        char mcp_path[4096];
        snprintf(mcp_path, sizeof mcp_path, "%s/mcp.json", path);
        FILE *f = fopen(mcp_path, "rb");
        if (!f) continue;
        char buf[65536];
        size_t got = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[got] = '\0';
        json_t *mcp = json_parse(buf, NULL);
        if (mcp && mcp->type == JSON_OBJECT) {
            json_t *servers = json_obj_get(mcp, "mcpServers");
            if (!servers) servers = json_obj_get(mcp, "mcp_servers");
            if (!servers) servers = mcp;
            if (servers->type == JSON_OBJECT) {
                for (size_t s = 0; s < json_object_size(servers); s++) {
                    const char *sname = json_object_get_key_at(servers, s);
                    json_t *sval = json_object_get_at(servers, s);
                    if (!sname || !sval || sval->type != JSON_OBJECT) continue;
                    json_set(out, sname, json_copy(sval));  /* defensive copy */
                }
            }
        }
        if (mcp) json_free(mcp);
    }
    plugins_scan_free(r);
    if (cfg) json_free(cfg);
    return out;
}

/* PoP: plugin_get_portable_mcp_servers @ hermes_cli/plugins.py:get_portable_mcp_servers */
void plugin_get_portable_mcp_servers(const char *raw_config_json,
                                     char *out_json, size_t out_sz) {
    json_t *servers = pg_portable_mcp_servers(raw_config_json);
    char *text = json_serialize(servers);
    json_free(servers);
    snprintf(out_json, out_sz, "%s", text ? text : "{}");
    free(text);
}

/* PoP: plugin_has_portable_mcp_servers @ hermes_cli/plugins.py:has_portable_mcp_servers */
int plugin_has_portable_mcp_servers(const char *raw_config_json) {
    json_t *servers = pg_portable_mcp_servers(raw_config_json);
    int any = json_object_size(servers) > 0 ? 1 : 0;
    json_free(servers);
    return any;
}

/* PoP: plugin_toolset_keys_cache_path @ hermes_cli/plugins.py:_plugin_toolset_keys_cache_path */
void plugin_toolset_keys_cache_path(char *out, size_t out_sz) {
    char *path = plugins_toolset_keys_cache_path();
    snprintf(out, out_sz, "%s", path ? path : "");
    free(path);
}

/* PoP: plugin_persist_toolset_keys @ hermes_cli/plugins.py:_persist_plugin_toolset_keys */
void plugin_persist_toolset_keys(const char *const *toolset_keys,
                                 size_t n_toolset_keys,
                                 const char *const *portable_mcp,
                                 size_t n_portable_mcp) {
    plugins_persist_toolset_keys(toolset_keys, n_toolset_keys,
                                 portable_mcp, n_portable_mcp);
}

/* PoP: plugin_read_keys_cache @ hermes_cli/plugins.py:_read_plugin_keys_cache */
void plugin_read_keys_cache(char *out_json, size_t out_sz) {
    char *blob = plugins_read_keys_cache();
    snprintf(out_json, out_sz, "%s", blob ? blob : "{}");
    free(blob);
}

/*
 * Discovery in the C port is synchronous (no background thread), so the live
 * value is always available; the persisted set is the fallback when the cache
 * PoP: plugin_get_toolset_keys_nowait @ hermes_cli/plugins.py:get_plugin_toolset_keys_nowait
 */
void plugin_get_toolset_keys_nowait(char *out_json, size_t out_sz) {
    size_t n = 0;
    char **keys = plugins_cached_string_list("toolset_keys", &n);
    pg_emit_string_array((const char *const *)keys, n, out_json, out_sz);
    plugins_free_list(keys, n);
}

/* PoP: plugin_get_portable_mcp_names_nowait @ hermes_cli/plugins.py:get_portable_mcp_server_names_nowait */
void plugin_get_portable_mcp_names_nowait(char *out_json, size_t out_sz) {
    size_t n = 0;
    char **names = plugins_cached_string_list("portable_mcp", &n);
    pg_emit_string_array((const char *const *)names, n, out_json, out_sz);
    plugins_free_list(names, n);
}

/*
 * Registers a portable package's MCP servers into the caller-owned registry
 * JSON. Returns the server map declared by the package (empty when it has
 * PoP: plugin_load_portable @ hermes_cli/plugins.py:_load_portable_plugin
 */
void plugin_load_portable(const char *manifest_path, char *out_json,
                          size_t out_sz) {
    snprintf(out_json, out_sz, "{}");
    if (!manifest_path || !*manifest_path) return;
    char mcp_path[4096];
    snprintf(mcp_path, sizeof mcp_path, "%s/mcp.json", manifest_path);
    FILE *f = fopen(mcp_path, "rb");
    if (!f) return;
    char buf[65536];
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[got] = '\0';
    json_t *mcp = json_parse(buf, NULL);
    if (!mcp) return;
    if (mcp->type == JSON_OBJECT) {
        json_t *servers = json_obj_get(mcp, "mcpServers");
        if (!servers) servers = json_obj_get(mcp, "mcp_servers");
        if (!servers) servers = mcp;
        char *text = json_serialize(servers);
        if (text) snprintf(out_json, out_sz, "%s", text);
        free(text);
    }
    json_free(mcp);
}

/*
 * Progressive-disclosure metadata for the SKILL.md files each enabled plugin
 * ships under <plugin>/skills/<name>/SKILL.md, sorted by qualified name
 * PoP: plugin_list_skill_metadata @ hermes_cli/plugins.py:list_plugin_skill_metadata
 */
void plugin_list_skill_metadata(const char *raw_config_json, char *out_json,
                                size_t out_sz) {
    json_t *arr = json_array();
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    json_t *cfg = raw_config_json ? json_parse(raw_config_json, NULL) : NULL;
    json_t *plugins_cfg = (cfg && cfg->type == JSON_OBJECT)
                          ? json_obj_get(cfg, "plugins") : NULL;
    json_t *enabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                      ? json_obj_get(plugins_cfg, "enabled") : NULL;
    if (enabled && enabled->type != JSON_ARRAY) enabled = NULL;

    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        const char *key = plugins_scan_key(r, i);
        const char *name = plugins_scan_name(r, i);
        if (enabled) {
            bool listed = false;
            for (size_t j = 0; j < json_len(enabled); j++) {
                json_t *v = json_get(enabled, j);
                if (v && v->type == JSON_STRING && v->str_val &&
                    ((key && strcmp(v->str_val, key) == 0) ||
                     (name && strcmp(v->str_val, name) == 0))) listed = true;
            }
            if (!listed) continue;
        }
        const char *path = plugins_scan_path(r, i);
        if (!path || !key) continue;
        char skills_dir[4096];
        snprintf(skills_dir, sizeof skills_dir, "%s/skills", path);
        /* Each subdirectory with a SKILL.md is one qualified skill. */
        size_t sn = 0;
        char **subs = plugins_list_skill_dirs(skills_dir, &sn);
        for (size_t s = 0; s < sn; s++) {
            char qualified[512];
            snprintf(qualified, sizeof qualified, "%s:%s", key, subs[s]);
            json_t *o = json_object();
            json_set(o, "name", json_string(qualified));
            json_set(o, "description", json_string(""));
            json_set(o, "category", json_string("plugin"));
            json_set(o, "frontmatter", json_object());
            json_append(arr, o);
        }
        plugins_free_list(subs, sn);
    }
    plugins_scan_free(r);
    if (cfg) json_free(cfg);

    /* sorted(...) by qualified name */
    size_t n = json_len(arr);
    for (size_t i = 1; i < n; i++) {
        for (size_t j = i; j > 0; j--) {
            const char *a = json_get_str(json_get(arr, j - 1), "name", "");
            const char *b = json_get_str(json_get(arr, j), "name", "");
            if (strcmp(a, b) <= 0) break;
            json_t *tmp = arr->c.items[j - 1];
            arr->c.items[j - 1] = arr->c.items[j];
            arr->c.items[j] = tmp;
        }
    }

    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}


/* ── wave 8: plugin state / config pure helpers ───────────────────────── */

/* PoP: plugins_select_entry_point_group @ hermes_cli/plugins.py:_select_entry_point_group
 * Python: entry_points.select(group=group) when the API has .select, else a
 * dict lookup, else a filter over iterables of named tuples. In C the entry
 * points arrive as one JSON array of {group: ...} records, so all three
 * branches collapse to the same group filter. */
json_t *plugins_select_entry_point_group(const json_t *entry_points,
                                         const char *group) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (!entry_points || entry_points->type != JSON_ARRAY || !group) return out;
    for (size_t i = 0; i < entry_points->c.count; i++) {
        const json_t *ep = entry_points->c.items[i];
        if (!ep || ep->type != JSON_OBJECT) continue;
        const char *g = json_get_str(ep, "group", NULL);
        if (g && strcmp(g, group) == 0)
            json_append(out, json_copy(ep));
    }
    return out;
}

/* ^[A-Za-z0-9][A-Za-z0-9_-]{0,191}$ — fullmatch. */
static bool pc_portable_data_pattern(const char *s) {
    if (!s || !*s) return false;
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')
          || (c0 >= '0' && c0 <= '9')))
        return false;
    size_t n = strlen(s);
    if (n > 192) return false;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
              || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* PoP: plugins_plugin_data_namespace @ hermes_cli/plugins.py:_plugin_data_namespace */
char *plugins_plugin_data_namespace(const char *plugin_id,
                                    const char *skill_namespace) {
    const char *candidate = (skill_namespace && *skill_namespace)
                                ? skill_namespace
                                : plugin_id;
    /* Portable Agent Plugins already receive this exact PLUGIN_DATA path. */
    if (skill_namespace && *skill_namespace && candidate &&
        strncmp(candidate, "agent-plugin-", 13) == 0 &&
        pc_portable_data_pattern(candidate))
        return strdup(candidate);
    /* Reuse the portable namespace algorithm for native/nested ids too.
     * Its fixed prefix avoids Windows reserved device names while the
     * digest prevents collisions after unsafe characters are folded. */
    return plugins_portable_skill_namespace(candidate ? candidate : "");
}

/* PoP: plugins_nested_plugin_value @ hermes_cli/plugins.py:_nested_plugin_value */
const json_t *plugins_nested_plugin_value(const json_t *root,
                                          const char *const *segments,
                                          size_t n_segments,
                                          const json_t *default_val) {
    const json_t *current = root;
    for (size_t i = 0; i < n_segments; i++) {
        if (!current || current->type != JSON_OBJECT) return default_val;
        const json_t *next = json_obj_get(current, segments[i]);
        if (!next) return default_val;
        current = next;
    }
    return current;
}

/* PoP: plugins_nested_plugin_mapping @ hermes_cli/plugins.py:_nested_plugin_mapping */
json_t *plugins_nested_plugin_mapping(const char *const *segments,
                                      size_t n_segments,
                                      const json_t *value) {
    /* nested = value; for segment in reversed(segments): nested = {segment: nested} */
    json_t *nested = value ? json_copy(value) : json_null();
    if (!nested) return NULL;
    for (size_t i = n_segments; i > 0; i--) {
        json_t *obj = json_object();
        if (!obj) { json_free(nested); return NULL; }
        json_set(obj, segments[i - 1], nested);
        nested = obj;
    }
    return nested;
}

/* PoP: plugins_state_quota_bytes @ hermes_cli/plugins.py:PluginState.quota_bytes
 * _PLUGIN_STATE_QUOTA_BYTES = 10 * 1024 * 1024 */
size_t plugins_state_quota_bytes(void) { return 10u * 1024u * 1024u; }

/* PoP: plugins_validate_state_key @ hermes_cli/plugins.py:PluginState._validate_key
 * _PLUGIN_STATE_KEY_RE = ^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$ plus ".." ban.
 * Python raises ValueError; C callers check the boolean. */
bool plugins_validate_state_key(const char *key) {
    if (!key || !*key) return false;
    unsigned char c0 = (unsigned char)key[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')
          || (c0 >= '0' && c0 <= '9')))
        return false;
    size_t n = strlen(key);
    if (n > 128) return false;
    if (strstr(key, "..")) return false;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)key[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
              || (c >= '0' && c <= '9') || c == '_' || c == '-'
              || c == '.' || c == ':'))
            return false;
    }
    return true;
}

/* PoP: plugins_setting_segment_valid @ hermes_cli/plugins.py:_PLUGIN_SETTING_SEGMENT_RE.fullmatch
 * ^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$ */
bool plugins_setting_segment_valid(const char *segment) {
    if (!segment || !*segment) return false;
    unsigned char c0 = (unsigned char)segment[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')
          || (c0 >= '0' && c0 <= '9')))
        return false;
    size_t n = strlen(segment);
    if (n > 128) return false;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)segment[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
              || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* PoP: plugins_relative_segments @ hermes_cli/plugins.py:_plugin_relative_segments
 * Returns NULL (Python raises ValueError) on empty keys, '/', '\\', invalid
 * segments, or reserved roots ("model", "plugins", "security", "settings",
 * case-insensitive). Caller frees via plugins_free_list. */
char **plugins_relative_segments(const char *key, size_t *out_n) {
    if (out_n) *out_n = 0;
    if (!key || !*key) return NULL;
    if (strchr(key, '/') || strchr(key, '\\')) return NULL;
    bool reserved_root = false;
    static const char *const RESERVED_ROOTS[] = {
        "model", "plugins", "security", "settings", NULL
    };
    char **segs = NULL;
    size_t n = 0, cap = 0;
    const char *p = key;
    bool first = true;
    for (;;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        char *seg = malloc(len + 1);
        if (!seg) goto fail;
        memcpy(seg, p, len);
        seg[len] = '\0';
        if (!plugins_setting_segment_valid(seg)) { free(seg); goto fail; }
        if (first) {
            first = false;
            for (size_t r = 0; RESERVED_ROOTS[r]; r++) {
                /* segments[0].lower() in reserved roots */
                const char *s = seg;
                const char *rsv = RESERVED_ROOTS[r];
                bool eq = true;
                for (; *s && *rsv; s++, rsv++)
                    if (tolower((unsigned char)*s) != *rsv) { eq = false; break; }
                if (eq && !*s && !*rsv) { reserved_root = true; break; }
            }
        }
        if (n == cap) {
            cap = cap ? cap * 2 : 4;
            char **ns = realloc(segs, cap * sizeof *segs);
            if (!ns) { free(seg); goto fail; }
            segs = ns;
        }
        segs[n++] = seg;
        if (!dot) break;
        p = dot + 1;
    }
    if (reserved_root) goto fail;
    if (out_n) *out_n = n;
    return segs;
fail:
    plugins_free_list(segs, n);
    return NULL;
}

/* PoP: plugins_state_data_dir @ hermes_cli/plugins.py:PluginState.data_dir
 * get_hermes_home() / "plugin-data" / self._data_namespace */
char *plugins_state_data_dir(const char *data_namespace) {
    if (!data_namespace) data_namespace = "";
    const char *home_env = getenv("HERMES_HOME");
    char *home_dyn = NULL;
    const char *home;
    if (home_env && *home_env) {
        home = home_env;
    } else {
        home_dyn = pc_hermes_home();
        home = home_dyn ? home_dyn : "";
    }
    size_t need = strlen(home) + strlen("/plugin-data/") +
                  strlen(data_namespace) + 1;
    char *out = malloc(need);
    if (!out) { free(home_dyn); return NULL; }
    snprintf(out, need, "%s/plugin-data/%s", home, data_namespace);
    free(home_dyn);
    return out;
}

/* PoP: plugins_state_path @ hermes_cli/plugins.py:PluginState.path */
char *plugins_state_path(const char *data_namespace) {
    char *dir = plugins_state_data_dir(data_namespace);
    if (!dir) return NULL;
    size_t need = strlen(dir) + strlen("/state.json") + 1;
    char *out = malloc(need);
    if (!out) { free(dir); return NULL; }
    snprintf(out, need, "%s/state.json", dir);
    free(dir);
    return out;
}

/* PoP: plugins_state_lock_path @ hermes_cli/plugins.py:_locked_plugin_state.lock_path
 * path.with_name(f".{path.name}.lock") — the lock lives in a sibling file
 * because atomic replacement changes the target's inode/handle. */
char *plugins_state_lock_path(const char *path) {
    if (!path) return NULL;
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    size_t dir_len = slash ? (size_t)(slash - path) : 0;
    size_t need = dir_len + 2 + strlen(name) + strlen(".lock") + 1;
    char *out = malloc(need);
    if (!out) return NULL;
    if (dir_len)
        snprintf(out, need, "%.*s/.%s.lock", (int)dir_len, path, name);
    else
        snprintf(out, need, ".%s.lock", name);
    return out;
}


/* ── wave 9: config-schema validation, manifest v2 fields, serialized
 *    replacement decorator, module-origin resolution ─────────────────── */

/* SUPPORTED_MANIFEST_VERSION @ hermes_cli/plugins.py */
#define PLG_SUPPORTED_MANIFEST_VERSION 2

/* _CONFIG_SCHEMA_TYPES: logical type groups a declared "type" may map to. */
typedef enum {
    PLG_CT_STRING = 1 << 0,
    PLG_CT_INT    = 1 << 1,
    PLG_CT_FLOAT  = 1 << 2,
    PLG_CT_BOOL   = 1 << 3,
    PLG_CT_LIST   = 1 << 4,
    PLG_CT_DICT   = 1 << 5,
} plg_config_type_t;

/* Map a declared schema "type" string (lowercased) to its accepted group,
 * or 0 when unknown (type check skipped). Mirrors _CONFIG_SCHEMA_TYPES. */
static plg_config_type_t plg_config_schema_type(const char *stype) {
    if (!stype) return (plg_config_type_t)0;
    char lower[32];
    size_t n = strlen(stype);
    if (n >= sizeof(lower)) return (plg_config_type_t)0;
    for (size_t i = 0; i <= n; i++)
        lower[i] = (char)tolower((unsigned char)stype[i]);
    if (!strcmp(lower, "str") || !strcmp(lower, "string"))   return PLG_CT_STRING;
    if (!strcmp(lower, "int") || !strcmp(lower, "integer"))  return PLG_CT_INT;
    if (!strcmp(lower, "float") || !strcmp(lower, "number")) return PLG_CT_FLOAT;
    if (!strcmp(lower, "bool") || !strcmp(lower, "boolean")) return PLG_CT_BOOL;
    if (!strcmp(lower, "list") || !strcmp(lower, "array"))   return PLG_CT_LIST;
    if (!strcmp(lower, "dict") || !strcmp(lower, "object"))  return PLG_CT_DICT;
    return (plg_config_type_t)0;
}

/* Python's type(value).__name__ for a JSON node — used only inside the
 * human-actionable warning text. */
static const char *plg_json_pytype_name(const json_t *v) {
    if (!v || v->type == JSON_NULL)   return "NoneType";
    if (v->type == JSON_BOOL)         return "bool";
    if (v->type == JSON_STRING)       return "str";
    if (v->type == JSON_ARRAY)        return "list";
    if (v->type == JSON_OBJECT)       return "dict";
    /* JSON_NUMBER: bool is not an int subclass here, but int vs float must
     * be distinguished exactly as Python prints it. */
    double d = v->num_val;
    if (d == (double)(long long)d && d >= -9.007199254740992e15 &&
        d <= 9.007199254740992e15)
        return "int";
    return "float";
}

/* True when `value` satisfies the expected group. In this tree bools are
 * their own type (JSON_BOOL), so Python's "bool is an int subclass" hazard
 * cannot fire — a true/false never satisfies int/float, matching the
 * explicit guard in the Python source. */
static bool plg_value_matches(const json_t *value, plg_config_type_t expected) {
    if (!value) return false;
    switch (value->type) {
    case JSON_STRING: return (expected & PLG_CT_STRING) != 0;
    case JSON_NUMBER:
        if (expected & PLG_CT_FLOAT) return true;
        if (expected & PLG_CT_INT) {
            double d = value->num_val;
            return d == (double)(long long)d && d >= -9.007199254740992e15 &&
                   d <= 9.007199254740992e15;
        }
        return false;
    case JSON_BOOL:   return (expected & PLG_CT_BOOL) != 0;
    case JSON_ARRAY:  return (expected & PLG_CT_LIST) != 0;
    case JSON_OBJECT: return (expected & PLG_CT_DICT) != 0;
    default:          return false;
    }
}

/* Validate a plugin's config entry against its declared config_schema:
 * fresh JSON array of human-actionable warnings (caller frees); never
 * fails — mismatches must not block plugin load (#64165). */
/* PoP: plg_validate_config_schema @ hermes_cli/plugins.py:validate_config_schema */
json_t *plg_validate_config_schema(const char *plugin_id,
                                   const json_t *schema,
                                   const json_t *settings) {
    json_t *warnings = json_array();
    if (!warnings) return NULL;
    if (!json_is_object(schema) || !json_is_object(settings)) return warnings;

    size_t n = json_object_size(schema);
    for (size_t i = 0; i < n; i++) {
        const char *skey = json_object_get_key_at(schema, i);
        const json_t *spec = json_object_get_at(schema, i);
        if (!skey || !json_is_object(spec))
            continue;                       /* not a Mapping spec */

        const json_t *present = json_obj_get(settings, skey);
        if (!present) {
            const json_t *required = json_obj_get(spec, "required");
            if (json_is_true(required) && !json_obj_get(spec, "default")) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "plugins.entries.%s.settings.%s is required by the "
                         "plugin's config_schema but is not set",
                         plugin_id ? plugin_id : "", skey);
                json_append(warnings, json_string(msg));
            }
            continue;
        }

        const json_t *stype_node = json_obj_get(spec, "type");
        const char *stype = json_string_value_safe(stype_node);
        plg_config_type_t expected =
            stype ? plg_config_schema_type(stype) : (plg_config_type_t)0;
        if (expected) {
            if (!plg_value_matches(present, expected)) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "plugins.entries.%s.settings.%s should be %s "
                         "(got %s)", plugin_id ? plugin_id : "", skey,
                         stype ? stype : "", plg_json_pytype_name(present));
                json_append(warnings, json_string(msg));
            }
        }
    }
    return warnings;
}

/* Validate and normalize the manifest v2 fields (#64165): fresh JSON object
 * of PluginManifest kwargs (caller frees); problems warn on stderr only. */
/* PoP: plg_parse_manifest_v2_fields @ hermes_cli/plugins.py:_parse_manifest_v2_fields */
json_t *plg_parse_manifest_v2_fields(const json_t *data, const char *key) {
    json_t *out = json_object();
    if (!out) return NULL;

    /* manifest_version — absent means v1 (supported forever). */
    long mv = 1;
    const json_t *raw_mv = json_is_object(data)
                               ? json_obj_get(data, "manifest_version") : NULL;
    if (raw_mv) {
        if (json_is_number(raw_mv)) {
            mv = (long)raw_mv->num_val;
        } else {
            fprintf(stderr,
                    "WARNING Plugin %s: manifest_version is not an integer; "
                    "treating as 1\n", key ? key : "");
            mv = 1;
        }
    }
    if (mv > PLG_SUPPORTED_MANIFEST_VERSION)
        fprintf(stderr,
                "WARNING Plugin %s: manifest_version %ld is newer than this "
                "Hermes supports (%d); loading anyway and ignoring unknown "
                "fields\n", key ? key : "", mv, PLG_SUPPORTED_MANIFEST_VERSION);
    json_set(out, "manifest_version", json_number((double)mv));

    /* api_version — plugin API generation (independent of manifest_version). */
    const json_t *raw_api =
        json_is_object(data) ? json_obj_get(data, "api_version") : NULL;
    if (!raw_api || json_is_null(raw_api)) {
        json_set(out, "api_version", json_null());
    } else if (json_is_number(raw_api)) {
        json_set(out, "api_version",
                 json_int((long)raw_api->num_val));
    } else {
        fprintf(stderr,
                "WARNING Plugin %s: api_version is not an integer; ignoring\n",
                key ? key : "");
        json_set(out, "api_version", json_null());
    }

    /* requires_plugins — list of {id, version_range?} (str shorthand ok). */
    json_t *deps = json_array();
    if (!deps) { json_free(out); return NULL; }
    const json_t *raw_deps =
        json_is_object(data) ? json_obj_get(data, "requires_plugins") : NULL;
    bool deps_bad = raw_deps && !json_is_null(raw_deps) &&
                    !json_is_array(raw_deps);
    if (deps_bad)
        fprintf(stderr,
                "WARNING Plugin %s: requires_plugins must be a list; ignoring\n",
                key ? key : "");
    if (raw_deps && json_is_array(raw_deps)) {
        size_t nd = json_len(raw_deps);
        for (size_t i = 0; i < nd; i++) {
            const json_t *item = json_get(raw_deps, i);
            if (item && json_is_string(item)) {
                json_t *dep = json_object();
                if (!dep) continue;
                json_set(dep, "id", json_string(json_string_value(item)));
                json_set(dep, "version_range", json_null());
                json_append(deps, dep);
            } else if (item && json_is_object(item)) {
                const json_t *id = json_obj_get(item, "id");
                if (json_is_string(id) && json_string_value(id)[0]) {
                    const json_t *vr = json_obj_get(item, "version_range");
                    json_t *dep = json_object();
                    if (!dep) continue;
                    json_set(dep, "id", json_string(json_string_value(id)));
                    if (!vr || json_is_null(vr))
                        json_set(dep, "version_range", json_null());
                    else
                        json_set(dep, "version_range",
                                 json_string(plg_dep_vr_str(vr)));
                    json_append(deps, dep);
                } else {
                    fprintf(stderr,
                            "WARNING Plugin %s: requires_plugins entry must "
                            "be a plugin id string or a {id, version_range} "
                            "mapping; skipping\n", key ? key : "");
                }
            } else {
                fprintf(stderr,
                        "WARNING Plugin %s: requires_plugins entry must be a "
                        "plugin id string or a {id, version_range} mapping; "
                        "skipping\n", key ? key : "");
            }
        }
    }
    json_set(out, "requires_plugins", deps);

    /* python_dependencies — validated and surfaced ONLY; never auto-installed. */
    json_t *pydeps = json_array();
    if (!pydeps) { json_free(out); return NULL; }
    const json_t *raw_pydeps =
        json_is_object(data) ? json_obj_get(data, "python_dependencies") : NULL;
    if (raw_pydeps && !json_is_null(raw_pydeps) && !json_is_array(raw_pydeps))
        fprintf(stderr,
                "WARNING Plugin %s: python_dependencies must be a list of "
                "requirement strings; ignoring\n", key ? key : "");
    if (raw_pydeps && json_is_array(raw_pydeps)) {
        size_t np = json_len(raw_pydeps);
        for (size_t i = 0; i < np; i++) {
            const json_t *item = json_get(raw_pydeps, i);
            if (item && json_is_string(item)) {
                const char *s = json_string_value(item);
                while (*s == ' ' || *s == '\t' || *s == '\n' ||
                       *s == '\r') s++;                 /* lstrip */
                size_t len = strlen(s);
                while (len && (s[len-1]==' '||s[len-1]=='\t'||
                               s[len-1]=='\n'||s[len-1]=='\r')) len--; /*rstrip*/
                if (len) {
                    char *req = malloc(len + 1);
                    if (req) {
                        memcpy(req, s, len);
                        req[len] = '\0';
                        json_append(pydeps, json_string(req));
                        free(req);
                    }
                }
            } else {
                fprintf(stderr,
                        "WARNING Plugin %s: python_dependencies entry must be "
                        "a non-empty requirement string; skipping\n",
                        key ? key : "");
            }
        }
    }
    json_set(out, "python_dependencies", pydeps);

    /* config_schema — mapping of key -> {type?, default?, description?,
     * required?}. Unknown declared types warn; the check is skipped later. */
    json_t *schema = json_object();
    if (!schema) { json_free(out); return NULL; }
    const json_t *raw_schema =
        json_is_object(data) ? json_obj_get(data, "config_schema") : NULL;
    if (raw_schema && !json_is_null(raw_schema) && !json_is_object(raw_schema))
        fprintf(stderr,
                "WARNING Plugin %s: config_schema must be a mapping; ignoring\n",
                key ? key : "");
    if (raw_schema && json_is_object(raw_schema)) {
        size_t ns = json_object_size(raw_schema);
        for (size_t i = 0; i < ns; i++) {
            const char *skey = json_object_get_key_at(raw_schema, i);
            const json_t *spec = json_object_get_at(raw_schema, i);
            if (!spec || !json_is_object(spec)) {
                fprintf(stderr,
                        "WARNING Plugin %s: config_schema entry %s must be a "
                        "mapping (e.g. {type: str}); skipping\n",
                        key ? key : "", skey ? skey : "?");
                continue;
            }
            const json_t *stype_node = json_obj_get(spec, "type");
            const char *stype = json_string_value_safe(stype_node);
            if (stype && !plg_config_schema_type(stype))
                fprintf(stderr,
                        "WARNING Plugin %s: config_schema key %s declares "
                        "unknown type %s (known: array, bool, dict, float, "
                        "int, list, number, object, str, string); type check "
                        "will be skipped for it\n", key ? key : "",
                        skey ? skey : "?", stype);
            json_set(schema, skey, json_copy(spec));
        }
    }
    json_set(out, "config_schema", schema);

    /* Standard metadata. */
    const char *license_s = json_object_get_string(data, "license", "");
    json_set(out, "license", json_string(license_s ? license_s : ""));
    const char *homepage_s = json_object_get_string(data, "homepage", "");
    json_set(out, "homepage", json_string(homepage_s ? homepage_s : ""));

    json_t *tags = json_array();
    if (!tags) { json_free(out); return NULL; }
    const json_t *raw_tags =
        json_is_object(data) ? json_obj_get(data, "tags") : NULL;
    if (raw_tags && !json_is_null(raw_tags) && !json_is_array(raw_tags))
        fprintf(stderr, "WARNING Plugin %s: tags must be a list; ignoring\n",
                key ? key : "");
    if (raw_tags && json_is_array(raw_tags)) {
        size_t nt = json_len(raw_tags);
        for (size_t i = 0; i < nt; i++) {
            const json_t *t = json_get(raw_tags, i);
            if (t && json_is_string(t))
                json_append(tags, json_string(json_string_value(t)));
        }
    }
    json_set(out, "tags", tags);

    /* Forward compat: unknown fields warn (never fail). Keep v1 manifests
     * quiet at debug level — they predate the known-field census. */
    static const char *const KNOWN_MANIFEST_FIELDS[] = {
        /* v1 */
        "name", "version", "description", "author", "requires_env",
        "provides_tools", "provides_hooks", "kind", "hooks", "label",
        "optional_env", "platforms", "external_dependencies",
        "pip_dependencies", "provides_browser_providers",
        "provides_web_providers",
        /* v2 (#64165) */
        "manifest_version", "api_version", "requires_plugins",
        "python_dependencies", "config_schema", "license", "homepage", "tags",
        /* reserved for sibling sub-issues so their manifests don't warn */
        "capabilities", "emits", "listens", "hermes", "depends",
        NULL
    };
    char *unknown = NULL;
    size_t unknown_len = 0;
    if (json_is_object(data)) {
        size_t nk = json_object_size(data);
        for (size_t i = 0; i < nk; i++) {
            const char *k = json_object_get_key_at(data, i);
            if (!k) continue;
            bool known = false;
            for (size_t j = 0; KNOWN_MANIFEST_FIELDS[j]; j++)
                if (!strcmp(k, KNOWN_MANIFEST_FIELDS[j])) { known = true; break; }
            if (known) continue;
            size_t kl = strlen(k);
            char *nb = realloc(unknown, unknown_len + kl + 3);
            if (!nb) break;
            unknown = nb;
            if (unknown_len) unknown[unknown_len++] = ',';
            memcpy(unknown + unknown_len, k, kl);
            unknown_len += kl;
            unknown[unknown_len] = '\0';
        }
    }
    if (unknown && unknown_len) {
        if (mv >= 2)
            fprintf(stderr,
                    "WARNING Plugin %s: unknown manifest field(s) ignored: %s "
                    "(newer manifest schema or typo; plugin still loads)\n",
                    key ? key : "", unknown);
        else
            fprintf(stderr,
                    "DEBUG Plugin %s: unknown manifest field(s) ignored: %s "
                    "(newer manifest schema or typo; plugin still loads)\n",
                    key ? key : "", unknown);
    }
    free(unknown);

    return out;
}

/* str() of a version_range node: strings pass through, numbers/bools get
 * their Python repr spelling. Caller frees. */
static char *plg_dep_vr_str(const json_t *vr) {
    if (json_is_string(vr)) return pc_strdup(json_string_value(vr));
    char buf[64];
    if (json_is_bool(vr))
        snprintf(buf, sizeof(buf), "%s", vr->bool_val ? "True" : "False");
    else if (json_is_number(vr)) {
        double d = vr->num_val;
        if (d == (double)(long long)d)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
    } else
        snprintf(buf, sizeof(buf), "None");
    return pc_strdup(buf);
}

/* ── replacement_coordinator.transaction() parity ──────────────────────
 * registration_lifecycle.ReplacementCoordinator serializes registry
 * snapshot/write/acquire behind one re-entrant lock; _serialized_replacement
 * wraps a method so its body runs inside that lock. The C mirror holds the
 * coordinator mutex across the wrapped call. */
static pthread_mutex_t plg_replacement_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* Snapshot → write → lease attachment as one atomic transaction. */
/* PoP: plg_serialized_replacement @ hermes_cli/plugins.py:_serialized_replacement */
void *plg_serialized_replacement(void *(*method)(void *), void *args) {
    pthread_mutex_lock(&plg_replacement_lock);
    void *result = method ? method(args) : NULL;
    pthread_mutex_unlock(&plg_replacement_lock);
    return result;
}

/* Source path WITHOUT importing (NULL on anything unexpected); top-level
 * name via PYTHONPATH + cwd search, dotted parts walked by hand per the
 * PathFinder file-layout conventions. Caller frees. */
static bool plg_is_regular_file(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* PoP: plg_resolve_module_origin @ hermes_cli/plugins.py:resolve_module_origin */
char *plg_resolve_module_origin(const char *module_name) {
    if (!module_name) return NULL;

    /* parts = [p for p in module_name.split(".") if p] */
    char **parts = NULL;
    size_t n_parts = 0;
    const char *p = module_name;
    while (*p) {
        const char *dot = strchr(p, '.');
        size_t seg = dot ? (size_t)(dot - p) : strlen(p);
        if (seg) {
            char **np = realloc(parts, (n_parts + 1) * sizeof(char *));
            if (!np) { plugins_free_list(parts, n_parts); return NULL; }
            parts = np;
            parts[n_parts] = malloc(seg + 1);
            if (!parts[n_parts]) {
                plugins_free_list(parts, n_parts);
                return NULL;
            }
            memcpy(parts[n_parts], p, seg);
            parts[n_parts][seg] = '\0';
            n_parts++;
        }
        p = dot ? dot + 1 : p + seg;
    }
    if (n_parts == 0) { plugins_free_list(parts, n_parts); return NULL; }

    /* Search directories: $PYTHONPATH segments then "." (cwd), like sys.path. */
    char **dirs = NULL;
    size_t n_dirs = 0;
    bool dirs_failed = false;
    const char *pp = getenv("PYTHONPATH");
    if (pp && *pp) {
        const char *q = pp;
        while (*q) {
            const char *colon = strchr(q, ':');
            size_t seg = colon ? (size_t)(colon - q) : strlen(q);
            char **nd = realloc(dirs, (n_dirs + 1) * sizeof(char *));
            if (!nd) { dirs_failed = true; break; }
            dirs = nd;
            dirs[n_dirs] = malloc(seg + 1);
            if (!dirs[n_dirs]) { dirs_failed = true; break; }
            memcpy(dirs[n_dirs], q, seg);
            dirs[n_dirs][seg] = '\0';
            n_dirs++;
            q = colon ? colon + 1 : q + seg;
        }
    }
    if (!dirs_failed) {
        char **nd = realloc(dirs, (n_dirs + 1) * sizeof(char *));
        if (nd) {
            dirs = nd;
            dirs[n_dirs] = pc_strdup(".");
            if (dirs[n_dirs]) n_dirs++;
        }
    }

    char *result = NULL;
    char *search_dir = NULL;   /* current submodule_search_locations (single dir) */
    bool have_search_dir = false;

    for (size_t pi = 0; pi < n_parts; pi++) {
        const char *part = parts[pi];
        char *found_origin = NULL;
        char *next_dir = NULL;

        size_t ndir = have_search_dir ? 1 : n_dirs;
        for (size_t di = 0; di < ndir; di++) {
            const char *base = have_search_dir ? search_dir : dirs[di];

            size_t need = strlen(base) + 1 + strlen(part) +
                          strlen("/__init__.py") + 1;
            char *pkg_init = malloc(need);
            char *mod_file = malloc(strlen(base) + 1 + strlen(part) + 4);
            if (!pkg_init || !mod_file) {
                free(pkg_init); free(mod_file);
                continue;
            }
            snprintf(pkg_init, need, "%s/%s/__init__.py", base, part);
            snprintf(mod_file, need, "%s/%s.py", base, part);

            if (plg_is_regular_file(pkg_init)) {
                found_origin = pkg_init;
                next_dir = malloc(strlen(base) + 1 + strlen(part) + 1);
                if (next_dir) snprintf(next_dir, strlen(base) + 1 +
                                       strlen(part) + 1, "%s/%s", base, part);
                else { free(pkg_init); found_origin = NULL; }
            } else if (plg_is_regular_file(mod_file)) {
                found_origin = mod_file;
            } else {
                free(pkg_init);
                free(mod_file);
            }
            if (found_origin) break;
        }

        if (!found_origin) goto done;      /* except Exception: return None */

        if (pi + 1 == n_parts || !next_dir) {
            result = found_origin;
            goto done;
        }
        free(search_dir);
        search_dir = next_dir;
        have_search_dir = true;
    }

done:
    free(search_dir);
    plugins_free_list(dirs, n_dirs);
    plugins_free_list(parts, n_parts);
    return result;
}
