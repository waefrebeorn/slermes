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
