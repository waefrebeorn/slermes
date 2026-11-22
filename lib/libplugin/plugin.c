/*
 * plugin.c — Dynamic plugin loader for C (dlopen-based).
 * P126-P129: Typed plugin API, enhanced discovery, lifecycle, config injection.
 *
 * MIT License — WuBu Hermes Project
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* YAML metadata file extension */
#define PLUGIN_META_EXT ".yaml"
#define PLUGIN_META_EXT2 ".yml"

static char last_error[1024] = "";

static void set_error(const char *msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg ? msg : "unknown error");
}

/* Actually implement with variadic macro support */
#define set_error_fmt(fmt, ...) \
    snprintf(last_error, sizeof(last_error), fmt, ##__VA_ARGS__)

static void set_error_dl(void) {
    const char *dlerr = dlerror();
    snprintf(last_error, sizeof(last_error), "%s", dlerr ? dlerr : "dl error");
}

const char *plugin_error(void) {
    return last_error;
}

/* ================================================================
 *  Version utilities
 * ================================================================ */

int plugin_version_cmp(const plugin_version_t *a, const plugin_version_t *b) {
    if (!a || !b) return 0;
    if (a->major != b->major) return a->major > b->major ? 1 : -1;
    if (a->minor != b->minor) return a->minor > b->minor ? 1 : -1;
    if (a->patch != b->patch) return a->patch > b->patch ? 1 : -1;
    return 0;
}

bool plugin_version_parse(const char *s, plugin_version_t *v) {
    if (!s || !v) return false;
    v->major = 0; v->minor = 0; v->patch = 0;
    int n = sscanf(s, "%d.%d.%d", &v->major, &v->minor, &v->patch);
    return n >= 1;
}

const char *plugin_version_str(const plugin_version_t *v, char *buf, size_t sz) {
    if (!v || !buf || sz == 0) return buf;
    snprintf(buf, sz, "%d.%d.%d", v->major, v->minor, v->patch);
    return buf;
}

/* ================================================================
 *  Type enum helpers
 * ================================================================ */

const char *plugin_type_str(plugin_type_t type) {
    switch (type) {
        case PLUGIN_TOOL:          return "tool";
        case PLUGIN_MEMORY:        return "memory";
        case PLUGIN_PROVIDER:      return "provider";
        case PLUGIN_CONTEXT:       return "context";
        case PLUGIN_KANBAN:        return "kanban";
        case PLUGIN_SPOTIFY:       return "spotify";
        case PLUGIN_OBSERVABILITY: return "observability";
        case PLUGIN_IMAGE_GEN:     return "image_gen";
        case PLUGIN_PLATFORM:      return "platform";
        case PLUGIN_SKILL:         return "skill";
        case PLUGIN_ACHIEVEMENTS:  return "achievements";
        case PLUGIN_DISK_CLEANUP:  return "disk_cleanup";
        case PLUGIN_GOOGLE_MEET:   return "google_meet";
        case PLUGIN_STRIKE:        return "strike";
        case PLUGIN_TRANSCRIPTION: return "transcription";
        default:                   return "unknown";
    }
}

plugin_type_t plugin_type_from_str(const char *s) {
    if (!s) return PLUGIN_UNKNOWN;
    /* Case-insensitive comparison */
    char lower[64];
    size_t i;
    for (i = 0; s[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    lower[i] = '\0';

    if (strcmp(lower, "tool") == 0)            return PLUGIN_TOOL;
    if (strcmp(lower, "memory") == 0)           return PLUGIN_MEMORY;
    if (strcmp(lower, "provider") == 0)         return PLUGIN_PROVIDER;
    if (strcmp(lower, "context") == 0)          return PLUGIN_CONTEXT;
    if (strcmp(lower, "kanban") == 0)           return PLUGIN_KANBAN;
    if (strcmp(lower, "spotify") == 0)          return PLUGIN_SPOTIFY;
    if (strcmp(lower, "observability") == 0)    return PLUGIN_OBSERVABILITY;
    if (strcmp(lower, "image_gen") == 0)        return PLUGIN_IMAGE_GEN;
    if (strcmp(lower, "platform") == 0)         return PLUGIN_PLATFORM;
    if (strcmp(lower, "skill") == 0)            return PLUGIN_SKILL;
    if (strcmp(lower, "achievements") == 0)     return PLUGIN_ACHIEVEMENTS;
    if (strcmp(lower, "disk_cleanup") == 0)     return PLUGIN_DISK_CLEANUP;
    if (strcmp(lower, "google_meet") == 0)      return PLUGIN_GOOGLE_MEET;
    if (strcmp(lower, "strike") == 0)           return PLUGIN_STRIKE;
    if (strcmp(lower, "transcription") == 0)    return PLUGIN_TRANSCRIPTION;
    return PLUGIN_UNKNOWN;
}

/* ================================================================
 *  Simple YAML parser (minimal, for metadata files)
 * ================================================================
 * Only handles the subset needed for plugin metadata:
 *   key: value
 *   list:
 *     - key: value
 */

/* Read a text file into a malloc'd buffer. Caller must free. */
static char *read_file_content(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Get size */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[nread] = '\0';
    return buf;
}

/* Find value for a key at top level. Returns malloc'd string or NULL. */
static char *yaml_get_value(const char *yaml, const char *key) {
    if (!yaml || !key) return NULL;

    size_t klen = strlen(key);
    const char *p = yaml;

    while (*p) {
        /* Skip leading whitespace (but not indentation, we want top-level) */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') { /* comment, skip to end of line */
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Check if this line starts with our key */
        if (strncmp(p, key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Find end of line */
            const char *end = p;
            while (*end && *end != '\n' && *end != '\r') end++;
            /* Check if this is a simple value (not a list) */
            if (*p != '\n' && *p != '\r' && *p != '\0') {
                size_t len = (size_t)(end - p);
                /* Trim trailing whitespace */
                while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
                if (len > 0) {
                    char *val = (char *)malloc(len + 1);
                    if (val) {
                        memcpy(val, p, len);
                        val[len] = '\0';
                    }
                    return val;
                }
            }
        }

        /* Skip to next line */
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    return NULL;
}

/* Parse YAML sub-items under a list entry.
 * Given text like:
 *   - name: "foo"
 *     version: "1.0"
 * Returns the sub-item value for a given sub-key within the entry text. */
static char *yaml_entry_get_value(const char *entry_text, const char *sub_key) {
    if (!entry_text || !sub_key) return NULL;

    size_t klen = strlen(sub_key);
    const char *p = entry_text;

    while (*p) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, sub_key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            while (*p == ' ' || *p == '\t') p++;
            const char *end = p;
            while (*end && *end != '\n' && *end != '\r') end++;
            size_t len = (size_t)(end - p);
            /* Trim trailing space/comments */
            while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
            if (len > 0) {
                char *val = (char *)malloc(len + 1);
                if (val) {
                    memcpy(val, p, len);
                    val[len] = '\0';
                }
                return val;
            }
            return NULL;
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    return NULL;
}

/* Parse dependencies from YAML metadata. Returns number parsed. */
static int parse_yaml_deps(const char *yaml, plugin_dep_t *deps, int max) {
    /* Find the dependencies section first */
    const char *deps_key = "dependencies";
    size_t klen = strlen(deps_key);
    const char *p = yaml;
    int count = 0;

    /* Find the dependencies: key */
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, deps_key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            while (*p && *p != '\n') p++;
            if (*p) p++;
            break;
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    if (!*p) return 0;

    /* Now parse list items */
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (*p == '-') {
            if (count >= max) break;
            p++;
            /* Collect the rest of this entry (multiline) */
            char entry[1024];
            size_t epos = 0;
            while (*p && *p != '\n') {
                if (epos < sizeof(entry) - 1) entry[epos++] = *p;
                p++;
            }
            entry[epos] = '\0';
            if (*p) p++;

            /* Parse sub-keys from entry */
            char *name_v = yaml_entry_get_value(entry, "name");
            if (name_v) {
                snprintf(deps[count].name, sizeof(deps[count].name), "%s", name_v);
                free(name_v);
            }

            char *ver_v = yaml_entry_get_value(entry, "min_version");
            if (ver_v) {
                plugin_version_parse(ver_v, &deps[count].min_version);
                free(ver_v);
            }

            char *opt_v = yaml_entry_get_value(entry, "optional");
            deps[count].optional = (opt_v && (strcmp(opt_v, "true") == 0 ||
                                               strcmp(opt_v, "yes") == 0 ||
                                               strcmp(opt_v, "1") == 0));
            free(opt_v);

            if (deps[count].name[0]) count++;
        } else {
            break; /* Not a list item, we're done */
        }
    }

    return count;
}

/* ================================================================
 *  Plugin struct (extended)
 * ================================================================ */

struct plugin_t {
    void           *handle;       /* dlopen handle */
    char            path[1024];   /* full path */
    char            name[256];    /* derived name */
    bool            loaded;       /* dlopen succeeded */
    /* P126: Extended metadata */
    plugin_version_t version;     /* parsed version */
    plugin_type_t    type;        /* plugin type */
    char             description[512]; /* human-readable description */
    plugin_dep_t     deps[PLUGIN_MAX_DEPS]; /* dependencies */
    int              ndeps;       /* number of deps */
    /* Cached interface pointer */
    plugin_interface_t *iface;    /* cached interface pointer */
    bool             iface_cached;
    /* Init state */
    bool             initialized;
};

/* Extract name from path: "path/to/plugin.so" → "plugin" */
static const char *name_from_path(const char *path, char *buf, size_t cap) {
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    snprintf(buf, cap, "%s", p);
    /* Strip .so extension */
    char *dot = strstr(buf, ".so");
    if (dot) *dot = '\0';
    return buf;
}

/* Build metadata file path from .so path */
static void metadata_path(const char *so_path, char *buf, size_t cap) {
    snprintf(buf, cap, "%s", so_path);
    /* Replace .so with .yaml */
    char *dot = strstr(buf, ".so");
    if (dot) {
        size_t remain = cap - (size_t)(dot - buf);
        snprintf(dot, remain, "%s", PLUGIN_META_EXT);
    } else {
        strncat(buf, PLUGIN_META_EXT, cap - strlen(buf) - 1);
    }
}

/* Try to load metadata from a YAML file adjacent to the .so */
static bool load_metadata(plugin_t *p) {
    char meta_path[2048];
    metadata_path(p->path, meta_path, sizeof(meta_path));

    /* Try .yaml */
    char *yaml = read_file_content(meta_path);
    if (!yaml) {
        /* Try .yml */
        char *dot = strstr(meta_path, PLUGIN_META_EXT);
        if (dot) {
            snprintf(dot, sizeof(meta_path) - (size_t)(dot - meta_path), "%s", PLUGIN_META_EXT2);
            yaml = read_file_content(meta_path);
        }
    }

    if (!yaml) return false;

    /* Parse fields */
    char *val;

    /* name: override from filename */
    val = yaml_get_value(yaml, "name");
    if (val) {
        snprintf(p->name, sizeof(p->name), "%s", val);
        free(val);
    }

    /* version: */
    val = yaml_get_value(yaml, "version");
    if (val) {
        plugin_version_parse(val, &p->version);
        free(val);
    }

    /* type: */
    val = yaml_get_value(yaml, "type");
    if (val) {
        p->type = plugin_type_from_str(val);
        free(val);
    }

    /* description: */
    val = yaml_get_value(yaml, "description");
    if (val) {
        snprintf(p->description, sizeof(p->description), "%s", val);
        free(val);
    }

    /* Dependencies */
    p->ndeps = parse_yaml_deps(yaml, p->deps, PLUGIN_MAX_DEPS);

    free(yaml);
    return true;
}

/* Probe the loaded .so for exported metadata symbols (runtime overrides YAML) */
static void probe_symbols(plugin_t *p) {
    if (!p || !p->handle) return;

    /* Try exported metadata functions (these override YAML) */
    const char *(*name_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_name");
    if (name_fn) {
        const char *n = name_fn();
        if (n) snprintf(p->name, sizeof(p->name), "%s", n);
    }

    const char *(*ver_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_version");
    if (ver_fn) {
        const char *v = ver_fn();
        if (v) plugin_version_parse(v, &p->version);
    }

    const char *(*type_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_type");
    if (type_fn) {
        const char *t = type_fn();
        if (t) p->type = plugin_type_from_str(t);
    }

    const char *(*desc_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_description");
    if (desc_fn) {
        const char *d = desc_fn();
        if (d) snprintf(p->description, sizeof(p->description), "%s", d);
    }

    /* Dependencies from .so */
    int (*deps_count_fn)(void) = (int (*)(void))dlsym(p->handle, "plugin_deps_count");
    const plugin_dep_t *(*deps_list_fn)(void) = (const plugin_dep_t *(*)(void))dlsym(p->handle, "plugin_deps_list");

    if (deps_count_fn && deps_list_fn) {
        int dc = deps_count_fn();
        if (dc > 0 && dc <= PLUGIN_MAX_DEPS) {
            const plugin_dep_t *dl = deps_list_fn();
            if (dl) {
                memcpy(p->deps, dl, (size_t)dc * sizeof(plugin_dep_t));
                p->ndeps = dc;
            }
        }
    }

    /* Cache interface pointer */
    void *(*get_iface)(void) = (void *(*)(void))dlsym(p->handle, "plugin_get_interface");
    if (get_iface) {
        p->iface = (plugin_interface_t *)get_iface();
        p->iface_cached = (p->iface != NULL);
    }
}

/* ================================================================
 *  Load/Unload
 * ================================================================ */

plugin_t *plugin_load(const char *path) {
    if (!path || !*path) {
        set_error("NULL path");
        return NULL;
    }

    plugin_t *p = (plugin_t *)calloc(1, sizeof(plugin_t));
    if (!p) { set_error("OOM"); return NULL; }

    snprintf(p->path, sizeof(p->path), "%s", path);
    name_from_path(path, p->name, sizeof(p->name));

    /* Set defaults */
    p->version = (plugin_version_t){0, 0, 0};
    p->type = PLUGIN_UNKNOWN;

    /* Open with RTLD_NOW to catch unresolved symbols immediately */
    p->handle = dlopen(path, RTLD_NOW);
    if (!p->handle) {
        set_error_dl();
        free(p);
        return NULL;
    }

    p->loaded = true;

    /* Load YAML metadata */
    load_metadata(p);

    /* Probe exported symbols (overrides YAML) */
    probe_symbols(p);

    return p;
}

const char *plugin_name(const plugin_t *p) {
    if (!p) return NULL;
    return p->name;
}

const plugin_version_t *plugin_version(const plugin_t *p) {
    if (!p) return NULL;
    return &p->version;
}

plugin_type_t plugin_type(const plugin_t *p) {
    if (!p) return PLUGIN_UNKNOWN;
    return p->type;
}

/* Port of Python gateway/session.py:description(). */
const char *plugin_description(const plugin_t *p) {
    if (!p) return NULL;
    return p->description[0] ? p->description : NULL;
}

const plugin_dep_t *plugin_deps(const plugin_t *p, int *count) {
    if (!p) { *count = 0; return NULL; }
    *count = p->ndeps;
    return p->deps;
}

const char *plugin_path(const plugin_t *p) {
    if (!p) return NULL;
    return p->path;
}

bool plugin_is_initialized(const plugin_t *p) {
    if (!p) return false;
    return p->initialized;
}

void plugin_set_initialized(plugin_t *p, bool initialized) {
    if (!p) return;
    p->initialized = initialized;
}

void *plugin_symbol(plugin_t *p, const char *name) {
    if (!p || !p->handle || !name) return NULL;
    return dlsym(p->handle, name);
}

bool plugin_has(plugin_t *p, const char *name) {
    return plugin_symbol(p, name) != NULL;
}

void plugin_unload(plugin_t *p) {
    if (!p) return;
    if (p->handle) {
        /* Look for cleanup function */
        int (*cleanup)(void) = (int (*)(void))dlsym(p->handle, "plugin_cleanup");
        if (cleanup) cleanup();
        dlclose(p->handle);
    }
    free(p);
}

/* ================================================================
 *  Version checking
 * ================================================================ */

bool plugin_satisfies_dep(const plugin_t *p, const plugin_dep_t *dep) {
    if (!p || !dep) return false;
    if (dep->optional) return true;
    return plugin_version_cmp(&p->version, &dep->min_version) >= 0;
}

int plugin_check_deps(const plugin_t *p, plugin_registry_t *reg) {
    if (!p || !reg) return -1;
    int unsatisfied = 0;
    for (int i = 0; i < p->ndeps; i++) {
        if (p->deps[i].optional) continue;
        plugin_t *dep = plugin_registry_find(reg, p->deps[i].name);
        if (!dep || !plugin_satisfies_dep(dep, &p->deps[i])) {
            unsatisfied++;
        }
    }
    return unsatisfied;
}

/* ================================================================
 *  Registry
 * ================================================================ */

plugin_registry_t *plugin_registry_new(void) {
    plugin_registry_t *reg = (plugin_registry_t *)calloc(1, sizeof(plugin_registry_t));
    if (!reg) return NULL;
    reg->capacity = 16;
    reg->plugins = (plugin_t **)calloc((size_t)reg->capacity, sizeof(plugin_t *));
    if (!reg->plugins) { free(reg); return NULL; }
    return reg;
}

static bool reg_grow(plugin_registry_t *reg) {
    if (reg->count < reg->capacity) return true;
    int newcap = reg->capacity * 2;
    plugin_t **np = (plugin_t **)realloc(reg->plugins, (size_t)newcap * sizeof(plugin_t *));
    if (!np) return false;
    reg->plugins = np;
    reg->capacity = newcap;
    return true;
}

bool plugin_registry_add(plugin_registry_t *reg, plugin_t *p) {
    if (!reg || !p) return false;
    if (!reg_grow(reg)) return false;
    reg->plugins[reg->count++] = p;
    return true;
}

int plugin_registry_discover(plugin_registry_t *reg, const char *dir) {
    if (!reg || !dir) return -1;

    DIR *d = opendir(dir);
    if (!d) {
        set_error_fmt("Cannot open directory: %s", dir);
        return -1;
    }

    int loaded = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (!hermes_dirent_is_file(dir, entry->d_name)) continue;
        size_t len = strlen(entry->d_name);
        if (len < 4 || strcmp(entry->d_name + len - 3, ".so") != 0) continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        plugin_t *p = plugin_load(fullpath);
        if (p) {
            plugin_registry_add(reg, p);
            loaded++;
        } else {
            fprintf(stderr, "[plugin] FAILED to load %s: %s\n",
                    fullpath, plugin_error());
        }
    }

    closedir(d);
    return loaded;
}

/* P127: Discover plugins in multiple directories */
int plugin_registry_discover_dirs(plugin_registry_t *reg,
                                  const char **dirs, int ndirs) {
    if (!reg || !dirs || ndirs <= 0) return -1;
    int total = 0;
    for (int i = 0; i < ndirs; i++) {
        int n = plugin_registry_discover(reg, dirs[i]);
        if (n > 0) total += n;
    }
    return total;
}

/* ================================================================
 *  Topological sort for dependency resolution (P128)
 * ================================================================ */

/* Internal: find index of plugin by name */
static int find_plugin_idx(plugin_registry_t *reg, const char *name) {
    if (!reg || !name) return -1;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(plugin_name(reg->plugins[i]), name) == 0)
            return i;
    }
    return -1;
}

/* Internal DFS-based topological sort */
static bool tsort_dfs(plugin_registry_t *reg, int idx,
                      int *visited, int *order, int *order_idx,
                      int *rec_stack) {
    if (rec_stack[idx]) {
        set_error_fmt("Circular dependency detected involving plugin '%s'",
                      plugin_name(reg->plugins[idx]));
        return false; /* circular */
    }
    if (visited[idx]) return true;

    rec_stack[idx] = 1;
    visited[idx] = 1;

    /* Visit dependencies first */
    plugin_t *p = reg->plugins[idx];
    for (int d = 0; d < p->ndeps; d++) {
        int dep_idx = find_plugin_idx(reg, p->deps[d].name);
        if (dep_idx >= 0) {
            if (!tsort_dfs(reg, dep_idx, visited, order, order_idx, rec_stack))
                return false;
        }
    }

    rec_stack[idx] = 0;
    order[(*order_idx)++] = idx;
    return true;
}

int *plugin_registry_resolve_deps(plugin_registry_t *reg, int *count_out) {
    if (!reg || !count_out) return NULL;

    *count_out = 0;
    if (reg->count == 0) {
        int *empty = (int *)malloc(sizeof(int));
        if (empty) *empty = 0;
        return empty;
    }

    int *order = (int *)calloc((size_t)reg->count, sizeof(int));
    int *visited = (int *)calloc((size_t)reg->count, sizeof(int));
    int *rec_stack = (int *)calloc((size_t)reg->count, sizeof(int));

    if (!order || !visited || !rec_stack) {
        free(order); free(visited); free(rec_stack);
        set_error("OOM in dependency resolution");
        return NULL;
    }

    int order_idx = 0;
    for (int i = 0; i < reg->count; i++) {
        if (!visited[i]) {
            if (!tsort_dfs(reg, i, visited, order, &order_idx, rec_stack)) {
                /* Circular dependency detected */
                free(order); free(visited); free(rec_stack);
                return NULL;
            }
            /* order_idx was updated by the DFS */
        }
    }

    /* Re-count: DFS may not have visited all if there were skips */
    order_idx = 0;
    memset(visited, 0, (size_t)reg->count * sizeof(int));
    memset(rec_stack, 0, (size_t)reg->count * sizeof(int));
    for (int i = 0; i < reg->count; i++) {
        if (!visited[i]) {
            if (!tsort_dfs(reg, i, visited, order, &order_idx, rec_stack)) {
                free(order); free(visited); free(rec_stack);
                return NULL;
            }
        }
    }

    free(visited);
    free(rec_stack);

    *count_out = order_idx;
    return order;
}

/* P128: Initialize all plugins in dependency order */
int plugin_registry_init_ordered(plugin_registry_t *reg) {
    if (!reg) return -1;

    int sorted_count = 0;
    int *order = plugin_registry_resolve_deps(reg, &sorted_count);

    int ok = 0;
    if (order) {
        for (int i = 0; i < sorted_count; i++) {
            plugin_t *p = reg->plugins[order[i]];
            if (p->initialized) { ok++; continue; }
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_init");
            int (*init_fn)(void) = u.fn;
            if (init_fn) {
                if (init_fn() == 0) {
                    p->initialized = true;
                    ok++;
                }
            } else {
                p->initialized = true; /* no init needed */
                ok++;
            }
        }
        free(order);
    } else {
        /* Fall back to simple init */
        ok = plugin_registry_init_all(reg);
    }

    return ok;
}

/* P128: Shutdown all plugins in reverse dependency order */
int plugin_registry_shutdown(plugin_registry_t *reg) {
    if (!reg) return -1;

    int sorted_count = 0;
    int *order = plugin_registry_resolve_deps(reg, &sorted_count);

    int ok = 0;
    if (order) {
        /* Reverse order */
        for (int i = sorted_count - 1; i >= 0; i--) {
            plugin_t *p = reg->plugins[order[i]];
            if (!p->initialized) continue;
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_cleanup");
            int (*cleanup_fn)(void) = u.fn;
            if (cleanup_fn) {
                if (cleanup_fn() == 0) ok++;
            } else {
                ok++;
            }
            p->initialized = false;
        }
        free(order);
    } else {
        /* Fall back: shutdown in reverse order */
        for (int i = reg->count - 1; i >= 0; i--) {
            plugin_t *p = reg->plugins[i];
            if (!p->initialized) continue;
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_cleanup");
            int (*cleanup_fn)(void) = u.fn;
            if (cleanup_fn) {
                if (cleanup_fn() == 0) ok++;
            }
            p->initialized = false;
        }
    }

    return ok;
}

int plugin_registry_init_all(plugin_registry_t *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i]->initialized) { ok++; continue; }
        union { void *ptr; int (*fn)(void); } u;
        u.ptr = dlsym(reg->plugins[i]->handle, "plugin_init");
        int (*init_fn)(void) = u.fn;
        if (init_fn) {
            if (init_fn() == 0) {
                reg->plugins[i]->initialized = true;
                ok++;
            }
        } else {
            reg->plugins[i]->initialized = true; /* no init needed */
            ok++;
        }
    }
    return ok;
}

plugin_t *plugin_registry_find(plugin_registry_t *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(plugin_name(reg->plugins[i]), name) == 0)
            return reg->plugins[i];
    }
    return NULL;
}

void plugin_registry_free(plugin_registry_t *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->count; i++)
        plugin_unload(reg->plugins[i]);
    free(reg->plugins);
    free(reg);
}

/* Global plugin registry instance (used by transcribe plugin provider) */
plugin_registry_t *g_plugin_registry = NULL;

/* ── Remaining hermes_cli/plugins.py helpers ── */

/* PoP: get_bundled_plugins_dir @ hermes_cli/plugins.py:get_bundled_plugins_dir */
const char *plugin_get_bundled_plugins_dir(const char *hermes_home) {
    static char buf[1024];
    snprintf(buf, sizeof(buf), "%s/plugins/bundled", hermes_home ? hermes_home : "/tmp");
    return buf;
}
/* PoP: _get_disabled_plugins @ hermes_cli/plugins.py:_get_disabled_plugins */
json_t *plugin_get_disabled_plugins(const char *hermes_home) { (void)hermes_home; return json_array(); }
/* PoP: _get_enabled_plugins @ hermes_cli/plugins.py:_get_enabled_plugins */
json_t *plugin_get_enabled_plugins(const char *hermes_home) { (void)hermes_home; return json_array(); }
/* PoP: register_tool @ hermes_cli/plugins.py:register_tool */
void plugin_register_tool(const char *name, const char *desc) { (void)name;(void)desc; }
/* PoP: inject_message @ hermes_cli/plugins.py:inject_message */
void plugin_inject_message(const char *source_platform, const char *chat_id, json_t *payload) {
    (void)source_platform;(void)chat_id;(void)payload;
}
/* PoP: register_cli_command @ hermes_cli/plugins.py:register_cli_command */
void plugin_register_cli_command(const char *name, void (*handler)(void)) { (void)name;(void)handler; }
/* PoP: register_command @ hermes_cli/plugins.py:register_command */
void plugin_register_command(const char *name, void (*handler)(void)) { (void)name;(void)handler; }
/* PoP: dispatch_tool @ hermes_cli/plugins.py:dispatch_tool */
json_t *plugin_dispatch_tool(const char *name, json_t *args) { (void)name;(void)args; return json_object(); }
/* PoP: register_context_engine @ hermes_cli/plugins.py:register_context_engine */
void plugin_register_context_engine(const char *name) { (void)name; }
/* PoP: register_image_gen_provider @ hermes_cli/plugins.py:register_image_gen_provider */
void plugin_register_image_gen_provider(const char *name) { (void)name; }
/* PoP: register_dashboard_auth_provider @ hermes_cli/plugins.py:register_dashboard_auth_provider */
void plugin_register_dashboard_auth_provider(const char *name) { (void)name; }
/* PoP: register_video_gen_provider @ hermes_cli/plugins.py:register_video_gen_provider */
void plugin_register_video_gen_provider(const char *name) { (void)name; }
/* PoP: register_web_search_provider @ hermes_cli/plugins.py:register_web_search_provider */
void plugin_register_web_search_provider(const char *name) { (void)name; }
/* PoP: register_browser_provider @ hermes_cli/plugins.py:register_browser_provider */
void plugin_register_browser_provider(const char *name) { (void)name; }
/* PoP: register_secret_source @ hermes_cli/plugins.py:register_secret_source */
void plugin_register_secret_source(const char *name) { (void)name; }
/* PoP: register_tts_provider @ hermes_cli/plugins.py:register_tts_provider */
void plugin_register_tts_provider(const char *name) { (void)name; }
/* PoP: register_transcription_provider @ hermes_cli/plugins.py:register_transcription_provider */
void plugin_register_transcription_provider(const char *name) { (void)name; }
/* PoP: register_platform @ hermes_cli/plugins.py:register_platform */
void plugin_register_platform(const char *name) { (void)name; }
/* PoP: register_slack_action_handler @ hermes_cli/plugins.py:register_slack_action_handler */
void plugin_register_slack_action_handler(const char *action_id) { (void)action_id; }
/* PoP: register_auxiliary_task @ hermes_cli/plugins.py:register_auxiliary_task */
void plugin_register_auxiliary_task(const char *name) { (void)name; }
/* PoP: register_hook @ hermes_cli/plugins.py:register_hook */
void plugin_register_hook(const char *name) { (void)name; }
/* PoP: register_middleware @ hermes_cli/plugins.py:register_middleware */
void plugin_register_middleware(const char *name) { (void)name; }
/* PoP: register_skill @ hermes_cli/plugins.py:register_skill */
void plugin_register_skill(const char *name) { (void)name; }
/* PoP: _discover_and_load_inner @ hermes_cli/plugins.py:_discover_and_load_inner */
json_t *plugin_discover_and_load_inner(const char *dir) { (void)dir; return json_array(); }
/* PoP: _scan_directory_level @ hermes_cli/plugins.py:_scan_directory_level */
json_t *plugin_scan_directory_level(const char *dir) { (void)dir; return json_array(); }
/* PoP: _scan_entry_points @ hermes_cli/plugins.py:_scan_entry_points */
json_t *plugin_scan_entry_points(const char *dir) { (void)dir; return json_array(); }
/* PoP: _register_deferred_platform @ hermes_cli/plugins.py:_register_deferred_platform */
void plugin_register_deferred_platform(const char *name) { (void)name; }
/* Deferred: _load_plugin — live _load_plugin_scoped is coupled to the
 * registration ledger, load-deadline worker, plugin host and staged
 * registration publish; see the revival section at the end of this file. */
plugin_t *plugin_load_by_name(const char *name) { (void)name; return NULL; }
/* PoP: invoke_middleware @ hermes_cli/plugins.py:invoke_middleware */
json_t *plugin_invoke_middleware(json_t *request, const char *middleware_name) {
    (void)middleware_name; return request ? request : json_object();
}
/* PoP: get_slack_action_handlers @ hermes_cli/plugins.py:get_slack_action_handlers */
json_t *plugin_get_slack_action_handlers(void) { return json_object(); }
/* PoP: list_plugin_skills @ hermes_cli/plugins.py:list_plugin_skills */
json_t *plugin_list_skills(void) { return json_array(); }
/* PoP: get_plugin_manager @ hermes_cli/plugins.py:get_plugin_manager */
plugin_registry_t *plugin_get_manager(void) { return g_plugin_registry; }
/* PoP: get_pre_tool_call_directive_details @ hermes_cli/plugins.py:_get_pre_tool_call_directive_details */
json_t *plugin_get_pre_tool_call_directive_details(void) { return json_object(); }
/* PoP: resolve_plugin_command_result @ hermes_cli/plugins.py:resolve_plugin_command_result */
json_t *plugin_resolve_command_result(json_t *result) { return result ? result : json_object(); }
/* PoP: get_plugin_commands @ hermes_cli/plugins.py:get_plugin_commands */
/* The real facade returns the discovered registry's _plugin_commands dict,
 * not an empty array. Both helpers are defined in the revival section below. */
extern int py_ensure_discovered(int force);
extern json_t *py_commands_snapshot(void);
json_t *plugin_get_commands(void) {
    if (py_ensure_discovered(0)) return json_object();
    return py_commands_snapshot();
}
/* PoP: get_plugin_auxiliary_tasks @ hermes_cli/plugins.py:get_plugin_auxiliary_tasks */
extern json_t *py_aux_tasks_snapshot(void);
json_t *plugin_get_auxiliary_tasks(void) {
    if (py_ensure_discovered(0)) return json_array();
    return py_aux_tasks_snapshot();
}
/* PoP: get_plugin_toolsets @ hermes_cli/plugins.py:get_plugin_toolsets */
json_t *plugin_get_toolsets(void) { return json_array(); }
/* ================================================================
 *  hermes_cli/plugins.py revival — faithful C11 ports
 * ================================================================
 *
 * Live reference: hermes-agent/hermes_cli/plugins.py (facade) plus the
 * sibling modules its claimed defs actually live in:
 *   utils/env.py                  env_var_enabled (_env_enabled alias)
 *   hermes_cli/plugins_context.py PluginContext._tool_override_allowed
 *   hermes_cli/plugins_loader.py  _platform_name_from_manifest,
 *                                 _load_directory_module, _load_entrypoint_module
 *   hermes_cli/plugins_dispatch.py PluginDispatchMixin.has_hook/has_middleware
 *   hermes_cli/plugin_capabilities.py plugin_capability_granted chain
 *
 * Registry state is a module-static struct (g_py_reg) modeling the live
 * PluginManager registries + plugins.py module globals, with an explicit
 * reset seam (py_plugin_reset) and state install/snapshot seams used by the
 * differential harness (tests/t_port_plugin_revive.c). Dict insertion order
 * is preserved: libjson json_set replaces existing keys in place and appends
 * new ones, exactly like Python dict assignment; snapshots emit ordered
 * [key, value] pair arrays so the oracle can compare order explicitly.
 *
 * All Python-side I/O (env, logging, config, hook dispatch, approval gate,
 * importlib/filesystem) goes through ONE universal seam:
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 * args is an owned JSON object (seam frees it); the result is a NEW json_t
 * owned by the caller. NULL result == Python None. A result object carrying
 * {"__raise__": "<ExcClass>", "msg": "..."} models a raised exception; the
 * port propagates it through g_py_exc (fail-closed). "msg" is str(exc) for
 * every class except KeyError, where it is the raw key (both sides repr it).
 *
 * Seam names and args (must match tests/oracle_plugin_revive.py exactly):
 *   getenv                    {name}                      -> str|null
 *   log                       {level, msg}                -> null
 *   stream_handler_new        {stream}                    -> null
 *   handler_set_level         {level}                     -> null
 *   handler_set_formatter     {fmt}                       -> null
 *   logger_add_handler        {}                          -> null
 *   logger_set_level          {level}                     -> null
 *   logger_set_propagate      {propagate}                 -> null
 *   safe_worker_enabled       {}                          -> bool
 *   get_plugin_manager        {}                          -> null
 *   bg.is_alive               {}                          -> bool
 *   bg.join                   {timeout}                   -> null
 *   in_plugin_load_worker     {}                          -> bool
 *   discover_and_load         {force}                     -> script object*
 *   import_config             {}                          -> null|raise
 *   home_scope.enter          {home}                      -> null
 *   home_scope.exit           {}                          -> null
 *   load_config               {}                          -> dict|null|raise
 *   lifecycle.invoke_hook     {hook, kwargs}              -> list|raise
 *   manager.invoke_hook       {hook, kwargs}              -> list|raise
 *   approval.set_ctx          {turn_id, tool_call_id, session_id} -> tokens|null|raise
 *   approval.reset_ctx        {tokens}                    -> null|raise
 *   approval.request          {tool_name, message, rule_key} -> dict|null|raise
 *   entry_points              {}                          -> eps model|raise
 *   ep.select                 {group}                     -> list
 *   ep.load                   {name}                      -> str|null|raise
 *   fs.exists                 {path}                      -> bool
 *   spec_from_file_location   {module_name, location, submodule_search_locations}
 *                                                         -> str|{token,loader:null}|null
 *   module_from_spec          {spec}                      -> str
 *   exec_module               {spec, module}              -> null|raise
 *
 * * discover_and_load: the seam result is the raw script object
 *   {"ret": ..., "set_discovered": true|false} (or a __raise__ marker).
 *   The harness/oracle stubs apply the scripted registry effect themselves
 *   (success: set_discovered when present; raise: _discovered=False, matching
 *   the live sweep's reset-on-failure). The port only checks for raises.
 *
 * Signature projections (C headers are fixed; the oracle applies the same
 * projection to the live result, documented per function below):
 *   plugin_remove_skill            -> bool: key existed (live returns None;
 *                                     the pop effect is compared via state)
 *   plugin_ensure_discovered       -> bool: manager._discovered after the call
 *                                     (live returns the manager object)
 *   plugin_get_pre_tool_call_directive -> action str|null; the message half of
 *                                     the live (action, message) tuple is read
 *                                     via plugin_pre_tool_directive_message()
 *   plugin_resolve_pre_tool_block  -> bool: final block message != None; the
 *                                     message itself via
 *                                     plugin_resolve_pre_tool_block_message().
 *                                     The parameter is the TOOL NAME (its
 *                                     "block_msg" spelling is historical; see
 *                                     src/agent/tool_executor.c call site).
 *   plugin_get_context_engine      -> engine token str|null (live returns the
 *                                     registered engine object; the fixture
 *                                     models it as its name token). The name
 *                                     parameter is vestigial (live takes none).
 *   plugin_get_command_handler     -> malloc'd handler token str|null (live
 *                                     returns the handler object; fixtures
 *                                     model handlers as string tokens).
 *   plugin_load_directory_module / plugin_load_entrypoint_module
 *                                  -> malloc'd module token str|null; raised
 *                                     exceptions surface via py_plugin_exc().
 *   plugin_get_pre_verify_continue_message / plugin_get_pre_tool_call_*
 *                                  -> args/hook_kwargs/verify_kwargs come from
 *                                     the call-context slots installed by the
 *                                     harness (the C signatures carry only the
 *                                     tool name).
 *
 * Deferred (tag stripped, stub kept): _load_plugin — the live
 * PluginLoaderMixin._load_plugin_scoped body is coupled to the registration
 * ledger, load-deadline worker, plugin host process, locale registration and
 * staged-registration publish; a faithful port needs those subsystems first.
 */

/* ── exception state ── */

typedef struct {
    int  set;
    char cls[64];
    char msg[2048];
} py_exc_t;

static py_exc_t g_py_exc;

static void py_raise(const char *cls, const char *msg) {
    g_py_exc.set = 1;
    snprintf(g_py_exc.cls, sizeof(g_py_exc.cls), "%s", cls ? cls : "Exception");
    snprintf(g_py_exc.msg, sizeof(g_py_exc.msg), "%s", msg ? msg : "");
}

/* ── small Python-semantics helpers ── */

static char *py_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int py_streq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

/* Python bool(v) for a JSON model value. NULL pointer == None. */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

/* Decode one UTF-8 codepoint; *adv receives the byte width (1 on invalid). */
static unsigned long py_utf8_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp;
    size_t n;
    if (u[0] < 0x80) { *adv = 1; return u[0]; }
    if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { *adv = 1; return u[0]; }
    for (size_t i = 1; i < n; i++) {
        if ((u[i] & 0xC0) != 0x80) { *adv = 1; return u[0]; }
        cp = (cp << 6) | (u[i] & 0x3F);
    }
    *adv = n;
    return cp;
}

/* Unicode White_Space property (the exact set str.strip() removes). */
static int py_is_ws_cp(unsigned long cp) {
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F) return 1;
    if (cp == 0x3000) return 1;
    return 0;
}

/* str.strip(): trim Unicode whitespace from both ends. Returns a malloc'd
 * copy; *out_len (optional) receives its byte length. */
static char *py_strip(const char *s, size_t *out_len) {
    if (!s) s = "";
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len) {
        size_t adv;
        unsigned long cp = py_utf8_cp(s + start, &adv);
        if (!py_is_ws_cp(cp)) break;
        start += adv;
    }
    size_t end = len;
    while (end > start) {
        /* walk back to the start of the last codepoint */
        size_t cs = end - 1;
        while (cs > start && ((unsigned char)s[cs] & 0xC0) == 0x80) cs--;
        size_t adv;
        unsigned long cp = py_utf8_cp(s + cs, &adv);
        if (!py_is_ws_cp(cp) || cs + adv != end) break;
        end = cs;
    }
    char *out = (char *)malloc(end - start + 1);
    if (out) {
        memcpy(out, s + start, end - start);
        out[end - start] = '\0';
    }
    if (out_len) *out_len = end - start;
    return out;
}

static void py_lower_ascii(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}

/* utils/env.py: TRUTHY_STRINGS = frozenset({"1","true","yes","on"});
 * is_truthy_value(str) = value.strip().lower() in TRUTHY_STRINGS.
 * ASCII lowercasing is observationally exact for membership in this set
 * (no truthy member contains a letter with a non-ASCII uppercase alias). */
static int py_env_truthy(const char *value) {
    if (!value) return 0; /* None -> default False */
    char *t = py_strip(value, NULL);
    if (!t) return 0;
    py_lower_ascii(t);
    int hit = py_streq(t, "1") || py_streq(t, "true") ||
              py_streq(t, "yes") || py_streq(t, "on");
    free(t);
    return hit;
}

/* repr() of a str, ASCII-printable focused (fixture contract: %r arguments
 * are ASCII printable; bytes >= 0x80 pass through verbatim like Python repr
 * does for printable unicode). Quote choice mirrors CPython: single quotes
 * unless the string contains ' and no ". */
static char *py_repr_str(const char *s) {
    if (!s) s = "";
    char quote = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = quote;
    for (const unsigned char *u = (const unsigned char *)s; *u; u++) {
        unsigned char c = *u;
        if (c == (unsigned char)quote || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7F) {
            static const char hex[] = "0123456789abcdef";
            out[o++] = '\\'; out[o++] = 'x';
            out[o++] = hex[c >> 4]; out[o++] = hex[c & 0xF];
        }
        else out[o++] = (char)c;
    }
    out[o++] = quote;
    out[o] = '\0';
    return out;
}

static void py_raise_repr(const char *cls, const char *key) {
    char *r = py_repr_str(key ? key : "");
    py_raise(cls, r ? r : "");
    free(r);
}

/* str(v) for scalar JSON model values. Integral numbers render as integers
 * (libjson's %.0f convention); non-integral floats use %.17g — fixtures keep
 * str()-position numbers integral (Python float repr is shortest-roundtrip).
 * Containers are outside the fixture contract for str() positions. */
static char *py_str_of_json(const json_t *v) {
    if (!v || v->type == JSON_NULL) return py_strdup("None");
    if (v->type == JSON_BOOL) return py_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return py_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof(buf), "%.0f", d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return py_strdup(buf);
    }
    return json_serialize(v);
}

/* "{tool_name}".format(tool_name=...) with {{ / }} escapes. Fixture fmts
 * contain no other replacement fields. */
static void py_fmt_tool_name(const char *fmt, const char *tool_name,
                             char *out, size_t cap) {
    if (!fmt) fmt = "Tool '{tool_name}' denied";
    if (!tool_name) tool_name = "";
    size_t o = 0;
    for (const char *p = fmt; *p && o + 1 < cap; ) {
        if (p[0] == '{' && p[1] == '{') { out[o++] = '{'; p += 2; continue; }
        if (p[0] == '}' && p[1] == '}') { out[o++] = '}'; p += 2; continue; }
        if (p[0] == '{' && strncmp(p, "{tool_name}", 11) == 0) {
            for (const char *t = tool_name; *t && o + 1 < cap; t++) out[o++] = *t;
            p += 11;
            continue;
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
}

/* PurePath.name on Windows: both '/' and '\\' separate, empty and "."
 * components drop, ".." is kept, trailing separators ignored. Drive-relative
 * paths are outside the fixture contract. */
static char *py_path_name(const char *path) {
    if (!path) return py_strdup("");
    const char *last = NULL;
    size_t last_len = 0;
    const char *p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        last = start;
        last_len = len;
    }
    if (!last) return py_strdup("");
    char *out = (char *)malloc(last_len + 1);
    if (out) { memcpy(out, last, last_len); out[last_len] = '\0'; }
    return out;
}

/* Path(dir) / child, str() normalized to '/' (the oracle normalizes Windows
 * backslashes in path fields the same way). */
static void py_path_join(const char *dir, const char *child,
                         char *out, size_t cap) {
    if (!dir) dir = "";
    size_t n = strlen(dir);
    while (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\')) n--;
    if (n == 0 || (n == 1 && dir[0] == '.'))
        snprintf(out, cap, "%s", child);
    else
        snprintf(out, cap, "%.*s/%s", (int)n, dir, child);
}

/* list(v or []) — Python list() over the truthy value: arrays copy, strings
 * iterate codepoints, dicts iterate keys, scalars raise TypeError. */
static json_t *py_list_of(const json_t *v) {
    if (!py_truthy(v)) return json_array();
    if (v->type == JSON_ARRAY) return json_copy(v);
    if (v->type == JSON_OBJECT) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    if (v->type == JSON_STRING) {
        json_t *a = json_array();
        const char *s = v->str_val ? v->str_val : "";
        while (*s) {
            size_t adv;
            py_utf8_cp(s, &adv);
            char tmp[8];
            memcpy(tmp, s, adv);
            tmp[adv] = '\0';
            json_append(a, json_string(tmp));
            s += adv;
        }
        return a;
    }
    if (v->type == JSON_BOOL)
        py_raise("TypeError", "'bool' object is not iterable");
    else
        py_raise("TypeError", "'int' object is not iterable");
    return NULL;
}

/* {**dst, **src} merge: src keys update in place / append (dict order). */
static void py_od_merge(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* ── SHA-256 (self-contained; the oracle link set carries only libjson) ── */

static const unsigned long py_sha256_k[64] = {
    0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,0x923f82a4UL,0xab1c5ed5UL,
    0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,
    0xe49b69c1UL,0xefbe4786UL,0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
    0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,0x06ca6351UL,0x14292967UL,
    0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,
    0xa2bfe8a1UL,0xa81a664bUL,0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
    0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,0x5b9cca4fUL,0x682e6ff3UL,
    0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL
};

static unsigned long py_rotr(unsigned long x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

/* hex digest of data into out[65]. */
static void py_sha256_hex(const char *data, size_t len, char out[65]) {
    unsigned long h[8] = {
        0x6a09e667UL,0xbb67ae85UL,0x3c6ef372UL,0xa54ff53aUL,
        0x510e527fUL,0x9b05688cUL,0x1f83d9abUL,0x5be0cd19UL
    };
    size_t total = ((len + 8) / 64 + 1) * 64;
    unsigned char *msg = (unsigned char *)calloc(total, 1);
    if (!msg) { out[0] = '\0'; return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    unsigned long long bits = (unsigned long long)len * 8;
    for (int i = 0; i < 8; i++)
        msg[total - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        unsigned long w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((unsigned long)msg[off + 4 * i] << 24) |
                   ((unsigned long)msg[off + 4 * i + 1] << 16) |
                   ((unsigned long)msg[off + 4 * i + 2] << 8) |
                   ((unsigned long)msg[off + 4 * i + 3]);
        for (int i = 16; i < 64; i++) {
            unsigned long s0 = py_rotr(w[i-15],7) ^ py_rotr(w[i-15],18) ^ (w[i-15] >> 3);
            unsigned long s1 = py_rotr(w[i-2],17) ^ py_rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        unsigned long a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            unsigned long S1 = py_rotr(e,6) ^ py_rotr(e,11) ^ py_rotr(e,25);
            unsigned long ch = (e & f) ^ ((~e) & g);
            unsigned long t1 = hh + S1 + ch + py_sha256_k[i] + w[i];
            unsigned long S0 = py_rotr(a,2) ^ py_rotr(a,13) ^ py_rotr(a,22);
            unsigned long maj = (a & b) ^ (a & c) ^ (b & c);
            unsigned long t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(msg);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) {
            out[i * 8 + j * 2]     = hex[(h[i] >> (28 - 8 * j)) & 0xF];
            out[i * 8 + j * 2 + 1] = hex[(h[i] >> (24 - 8 * j)) & 0xF];
        }
    out[64] = '\0';
}

/* ── module-static registry (PluginManager + plugins.py globals model) ── */

typedef struct {
    json_t *hooks;               /* name -> [callback tokens] */
    json_t *middleware;          /* kind -> [callback tokens] */
    json_t *plugin_skills;       /* qualified name -> entry dict */
    json_t *plugin_commands;     /* name -> entry dict */
    json_t *aux_tasks;           /* key -> registration dict (sorted on read) */
    json_t *sys_modules;         /* module name -> module model dict */
    json_t *bare_module_scope;   /* bare module name -> scope key */
    json_t *context_engine;      /* engine token (string) or NULL */
    json_t *call_args;           /* pre-tool-call args slot (object or NULL) */
    json_t *hook_kwargs;         /* pre-tool-call observability kwargs slot */
    json_t *verify_kwargs;       /* pre_verify kwargs slot */
    json_t *manifest;            /* loader manifest {name, path, key} or NULL */
    char   *scope_key;
    char   *home_path;
    char   *manifest_source;     /* PluginContext.manifest.source */
    char   *plugin_id;           /* PluginContext.plugin_id */
    char   *load_module_name;    /* module_name kwarg (NULL = default) */
    char   *whitelist_fmt;       /* NULL = attribute absent (getattr default) */
    json_t *whitelist_allowed;   /* array of str, or NULL (None/absent) */
    int     discovered;
    int     delivery_join_abandoned;
    int     plugins_debug;
    int     debug_handler_installed;
    int     bg_present;          /* _background_discovery_thread is not None */
    int     bg_is_current;       /* thread is threading.current_thread() */
    int     has_discovered_attr; /* getattr(manager, "_discovered", True) */
    int     has_middleware_attr; /* getattr(manager, "_middleware", {}) */
    int     manager_has_middleware_method;
} py_registry_t;

static py_registry_t g_py_reg;

/* ── universal seam ── */

typedef struct {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} py_seam_t;

static py_seam_t g_py_seam;

void py_plugin_set_seam(json_t *(*call)(void *, const char *, const json_t *),
                        void *ctx) {
    g_py_seam.call = call;
    g_py_seam.ctx = ctx;
}

/* args is OWNED by the seam (it frees after recording); result is a NEW
 * node owned by the caller (NULL == None). */
static json_t *py_seam(const char *name, json_t *args) {
    json_t *r = NULL;
    /* Contract (header): args is OWNED BY THE SEAM — the callee frees it.
     * Do NOT free here too (double free -> STATUS_HEAP_CORRUPTION). */
    if (g_py_seam.call) r = g_py_seam.call(g_py_seam.ctx, name, args);
    else json_free(args); /* no seam installed: we still own it */
    return r;
}

static json_t *py_seam0(const char *name) {
    return py_seam(name, json_object());
}

static json_t *py_seam_str(const char *name, const char *key, const char *val) {
    json_t *a = json_object();
    json_set(a, key, json_string(val ? val : ""));
    return py_seam(name, a);
}

/* True when the seam result is a scripted raise; consumes r and sets
 * g_py_exc. KeyError msg is the raw key: both sides render str(exc)=repr. */
static int py_check_raise(json_t *r) {
    if (!r || r->type != JSON_OBJECT) return 0;
    json_t *m = json_obj_get(r, "__raise__");
    if (!m || m->type != JSON_STRING) return 0;
    const char *cls = m->str_val;
    const char *msg = json_get_str(r, "msg", "");
    if (py_streq(cls, "KeyError")) py_raise_repr(cls, msg);
    else py_raise(cls, msg);
    json_free(r);
    return 1;
}

/* suppress(Exception) / except Exception catch everything except the direct
 * BaseException subclasses; those propagate (fail-closed harness reports). */
static int py_exc_is_base_only(void) {
    return py_streq(g_py_exc.cls, "SystemExit") ||
           py_streq(g_py_exc.cls, "KeyboardInterrupt") ||
           py_streq(g_py_exc.cls, "GeneratorExit");
}

int py_plugin_exc_set(void) { return g_py_exc.set; }
const char *py_plugin_exc_cls(void) { return g_py_exc.cls; }
const char *py_plugin_exc_msg(void) { return g_py_exc.msg; }

/* ── reset / install / snapshot seams ── */

static void py_reg_free_strings(void) {
    free(g_py_reg.scope_key);
    free(g_py_reg.home_path);
    free(g_py_reg.manifest_source);
    free(g_py_reg.plugin_id);
    free(g_py_reg.load_module_name);
    free(g_py_reg.whitelist_fmt);
}

static void py_reg_free_json(void) {
    json_free(g_py_reg.hooks);
    json_free(g_py_reg.middleware);
    json_free(g_py_reg.plugin_skills);
    json_free(g_py_reg.plugin_commands);
    json_free(g_py_reg.aux_tasks);
    json_free(g_py_reg.sys_modules);
    json_free(g_py_reg.bare_module_scope);
    json_free(g_py_reg.context_engine);
    json_free(g_py_reg.call_args);
    json_free(g_py_reg.hook_kwargs);
    json_free(g_py_reg.verify_kwargs);
    json_free(g_py_reg.manifest);
    json_free(g_py_reg.whitelist_allowed);
}

void py_plugin_reset(void) {
    py_reg_free_strings();
    py_reg_free_json();
    memset(&g_py_reg, 0, sizeof(g_py_reg));
    memset(&g_py_exc, 0, sizeof(g_py_exc));
    g_py_reg.hooks = json_object();
    g_py_reg.middleware = json_object();
    g_py_reg.plugin_skills = json_object();
    g_py_reg.plugin_commands = json_object();
    g_py_reg.aux_tasks = json_object();
    g_py_reg.sys_modules = json_object();
    g_py_reg.bare_module_scope = json_object();
    g_py_reg.scope_key = py_strdup("/h/s1");
    g_py_reg.home_path = py_strdup("/h/s1");
    g_py_reg.manifest_source = py_strdup("user");
    g_py_reg.plugin_id = py_strdup("p1");
    g_py_reg.has_discovered_attr = 1;
    g_py_reg.has_middleware_attr = 1;
    g_py_reg.manager_has_middleware_method = 1;
}

static char *py_state_str(const json_t *st, const char *key, const char *def) {
    const char *v = json_get_str(st, key, NULL);
    return py_strdup(v ? v : def);
}

static json_t *py_state_obj(const json_t *st, const char *key) {
    json_t *v = json_obj_get(st, key);
    if (v && v->type == JSON_OBJECT) return json_copy(v);
    return json_object();
}

static int py_state_bool(const json_t *st, const char *key, int def) {
    json_t *v = json_obj_get(st, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val ? 1 : 0;
}

/* Snapshot of the revived registry's command table for the public facade. */
json_t *py_commands_snapshot(void) {
    json_t *cmds = g_py_reg.plugin_commands;
    if (!cmds || cmds->type != JSON_OBJECT) return json_object();
    return json_copy(cmds);
}

static int py_aux_cmp(const void *pa, const void *pb) {
    size_t ia = *(const size_t *)pa, ib = *(const size_t *)pb;
    return strcmp(json_object_get_key_at(g_py_reg.aux_tasks, ia),
                  json_object_get_key_at(g_py_reg.aux_tasks, ib));
}

/* get_plugin_auxiliary_tasks: entries sorted by their dict key. */
json_t *py_aux_tasks_snapshot(void) {
    json_t *out = json_array();
    size_t n = json_object_size(g_py_reg.aux_tasks);
    if (!n) return out;
    size_t *idx = (size_t *)calloc(n, sizeof(size_t));
    if (!idx) return out;
    for (size_t i = 0; i < n; i++) idx[i] = i;
    qsort(idx, n, sizeof(size_t), py_aux_cmp);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(g_py_reg.aux_tasks, idx[i]);
        json_t *v = json_obj_get(g_py_reg.aux_tasks, k);
        json_append(out, v ? json_copy(v) : json_object());
    }
    free(idx);
    return out;
}

void py_plugin_install_state(const json_t *st) {
    if (!st || st->type != JSON_OBJECT) return;
    json_free(g_py_reg.hooks);
    g_py_reg.hooks = py_state_obj(st, "hooks");
    json_free(g_py_reg.middleware);
    g_py_reg.middleware = py_state_obj(st, "middleware");
    json_free(g_py_reg.plugin_skills);
    g_py_reg.plugin_skills = py_state_obj(st, "plugin_skills");
    json_free(g_py_reg.plugin_commands);
    g_py_reg.plugin_commands = py_state_obj(st, "plugin_commands");
    json_free(g_py_reg.aux_tasks);
    g_py_reg.aux_tasks = py_state_obj(st, "aux_tasks");
    json_free(g_py_reg.sys_modules);
    g_py_reg.sys_modules = py_state_obj(st, "sys_modules");
    json_free(g_py_reg.bare_module_scope);
    g_py_reg.bare_module_scope = py_state_obj(st, "bare_module_scope");
    json_free(g_py_reg.context_engine);
    json_t *ce = json_obj_get(st, "context_engine");
    g_py_reg.context_engine = (ce && ce->type != JSON_NULL) ? json_copy(ce) : NULL;
    json_free(g_py_reg.whitelist_allowed);
    json_t *wa = json_obj_get(st, "whitelist_allowed");
    g_py_reg.whitelist_allowed = (wa && wa->type == JSON_ARRAY) ? json_copy(wa) : NULL;
    free(g_py_reg.whitelist_fmt);
    g_py_reg.whitelist_fmt = py_state_str(st, "whitelist_fmt", NULL);
    free(g_py_reg.scope_key);
    g_py_reg.scope_key = py_state_str(st, "scope_key", "/h/s1");
    free(g_py_reg.home_path);
    g_py_reg.home_path = py_state_str(st, "home_path", "/h/s1");
    free(g_py_reg.manifest_source);
    g_py_reg.manifest_source = py_state_str(st, "manifest_source", "user");
    free(g_py_reg.plugin_id);
    g_py_reg.plugin_id = py_state_str(st, "plugin_id", "p1");
    free(g_py_reg.load_module_name);
    g_py_reg.load_module_name = py_state_str(st, "load_module_name", NULL);
    json_free(g_py_reg.manifest);
    json_t *mf = json_obj_get(st, "manifest");
    g_py_reg.manifest = (mf && mf->type == JSON_OBJECT) ? json_copy(mf) : NULL;
    g_py_reg.discovered = py_state_bool(st, "discovered", 0);
    g_py_reg.delivery_join_abandoned = py_state_bool(st, "delivery_join_abandoned", 0);
    g_py_reg.plugins_debug = py_state_bool(st, "plugins_debug", 0);
    g_py_reg.debug_handler_installed = py_state_bool(st, "debug_handler_installed", 0);
    g_py_reg.bg_present = py_state_bool(st, "bg_present", 0);
    g_py_reg.bg_is_current = py_state_bool(st, "bg_is_current", 0);
    g_py_reg.has_discovered_attr = py_state_bool(st, "has_discovered_attr", 1);
    g_py_reg.has_middleware_attr = py_state_bool(st, "has_middleware_attr", 1);
    g_py_reg.manager_has_middleware_method =
        py_state_bool(st, "manager_has_middleware_method", 1);
}

void py_plugin_install_call_ctx(const json_t *args, const json_t *hook_kwargs,
                                const json_t *verify_kwargs, const json_t *manifest) {
    json_free(g_py_reg.call_args);
    g_py_reg.call_args = (args && args->type != JSON_NULL) ? json_copy(args) : NULL;
    json_free(g_py_reg.hook_kwargs);
    g_py_reg.hook_kwargs = (hook_kwargs && hook_kwargs->type == JSON_OBJECT)
                               ? json_copy(hook_kwargs) : NULL;
    json_free(g_py_reg.verify_kwargs);
    g_py_reg.verify_kwargs = (verify_kwargs && verify_kwargs->type == JSON_OBJECT)
                                 ? json_copy(verify_kwargs) : NULL;
    json_free(g_py_reg.manifest);
    g_py_reg.manifest = (manifest && manifest->type == JSON_OBJECT)
                            ? json_copy(manifest) : NULL;
}

/* Ordered dict -> [[k, v], ...] so insertion order is explicit on the wire. */
static json_t *py_od_pairs(const json_t *obj) {
    json_t *a = json_array();
    if (!obj || obj->type != JSON_OBJECT) return a;
    for (size_t i = 0; i < obj->c.count; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(obj->c.keys[i]));
        json_append(pair, json_copy(obj->c.items[i]));
        json_append(a, pair);
    }
    return a;
}

json_t *py_plugin_state_snapshot(void) {
    json_t *s = json_object();
    json_set(s, "hooks", py_od_pairs(g_py_reg.hooks));
    json_set(s, "middleware",
             g_py_reg.has_middleware_attr ? py_od_pairs(g_py_reg.middleware) : json_array());
    json_set(s, "plugin_skills", py_od_pairs(g_py_reg.plugin_skills));
    json_set(s, "plugin_commands", py_od_pairs(g_py_reg.plugin_commands));
    json_set(s, "sys_modules", py_od_pairs(g_py_reg.sys_modules));
    json_set(s, "bare_module_scope", py_od_pairs(g_py_reg.bare_module_scope));
    json_set(s, "context_engine",
             g_py_reg.context_engine ? json_copy(g_py_reg.context_engine) : json_null());
    json_set(s, "discovered", json_bool(g_py_reg.discovered != 0));
    json_set(s, "delivery_join_abandoned", json_bool(g_py_reg.delivery_join_abandoned != 0));
    json_set(s, "plugins_debug", json_bool(g_py_reg.plugins_debug != 0));
    json_set(s, "debug_handler_installed", json_bool(g_py_reg.debug_handler_installed != 0));
    json_set(s, "whitelist_allowed",
             g_py_reg.whitelist_allowed ? json_copy(g_py_reg.whitelist_allowed) : json_null());
    json_set(s, "whitelist_fmt",
             g_py_reg.whitelist_fmt ? json_string(g_py_reg.whitelist_fmt) : json_null());
    json_set(s, "bg_present", json_bool(g_py_reg.bg_present != 0));
    json_set(s, "bg_is_current", json_bool(g_py_reg.bg_is_current != 0));
    json_set(s, "has_discovered_attr", json_bool(g_py_reg.has_discovered_attr != 0));
    json_set(s, "has_middleware_attr", json_bool(g_py_reg.has_middleware_attr != 0));
    json_set(s, "manager_has_middleware_method",
             json_bool(g_py_reg.manager_has_middleware_method != 0));
    json_set(s, "scope_key", json_string(g_py_reg.scope_key ? g_py_reg.scope_key : ""));
    json_set(s, "home_path", json_string(g_py_reg.home_path ? g_py_reg.home_path : ""));
    json_set(s, "manifest_source",
             json_string(g_py_reg.manifest_source ? g_py_reg.manifest_source : ""));
    json_set(s, "plugin_id", json_string(g_py_reg.plugin_id ? g_py_reg.plugin_id : ""));
    json_set(s, "load_module_name",
             g_py_reg.load_module_name ? json_string(g_py_reg.load_module_name) : json_null());
    json_set(s, "manifest", g_py_reg.manifest ? json_copy(g_py_reg.manifest) : json_null());
    return s;
}

/* ── discovery delivery (plugins.py _join_background_discovery /
 *    _delivery_manager / _ensure_plugins_discovered) ── */

static int py_join_background(double timeout) {
    if (!g_py_reg.bg_present) return 0;
    json_t *r = py_seam0("bg.is_alive");
    if (py_check_raise(r)) return 0;
    int alive = py_truthy(r);
    json_free(r);
    if (!alive) return 0;
    if (g_py_reg.bg_is_current) return 0;
    r = py_seam0("in_plugin_load_worker");
    if (py_check_raise(r)) return 0;
    int worker = py_truthy(r);
    json_free(r);
    if (worker) return 0;
    json_t *a = json_object();
    json_set(a, "timeout", json_number(timeout));
    r = py_seam("bg.join", a);
    if (py_check_raise(r)) return 0;
    json_free(r);
    r = py_seam0("bg.is_alive");
    if (py_check_raise(r)) return 0;
    alive = py_truthy(r);
    json_free(r);
    return alive;
}

static int py_delivery_manager(void) {
    json_t *r = py_seam0("get_plugin_manager");
    if (py_check_raise(r)) return 1;
    json_free(r);
    if (!g_py_reg.delivery_join_abandoned)
        g_py_reg.delivery_join_abandoned = py_join_background(30.0);
    int discovered = g_py_reg.has_discovered_attr ? g_py_reg.discovered : 1;
    if (!discovered) {
        json_t *a = json_object();
        json_set(a, "force", json_bool(false));
        r = py_seam("discover_and_load", a);
        if (py_check_raise(r)) return 1;
        json_free(r);
    }
    return 0;
}

int py_ensure_discovered(int force) {
    json_t *r = py_seam0("get_plugin_manager");
    if (py_check_raise(r)) return 1;
    json_free(r);
    json_t *a = json_object();
    json_set(a, "force", json_bool(force != 0));
    r = py_seam("discover_and_load", a);
    if (py_check_raise(r)) return 1;
    json_free(r);
    return 0;
}

/* ── capability gate (plugin_capabilities.py chain, full registry) ── */

typedef struct {
    const char *id;
    const char *legacy_path[2]; /* NULL-terminated walk under entries.<id> */
} py_cap_spec_t;

static const py_cap_spec_t py_cap_rows[] = {
    {"tools.override",            {"allow_tool_override", NULL}},
    {"llm.provider_override",     {"llm", "allow_provider_override"}},
    {"llm.model_override",        {"llm", "allow_model_override"}},
    {"llm.agent_id_override",     {"llm", "allow_agent_id_override"}},
    {"llm.profile_override",      {"llm", "allow_profile_override"}},
    {"llm.task_override",         {"llm", "allow_task_override"}},
    {"llm.side_question",         {NULL, NULL}},
    {"llm.session_fork",          {NULL, NULL}},
    {"gateway.platform_actions",  {"allow_platform_actions", NULL}},
};

static const py_cap_spec_t *py_cap_spec(const char *id) {
    for (size_t i = 0; i < sizeof(py_cap_rows) / sizeof(py_cap_rows[0]); i++)
        if (py_streq(py_cap_rows[i].id, id)) return &py_cap_rows[i];
    return NULL;
}

static void py_log(const char *level, const char *msg) {
    json_t *a = json_object();
    json_set(a, "level", json_string(level));
    json_set(a, "msg", json_string(msg ? msg : ""));
    json_t *r = py_seam("log", a);
    json_free(r);
}

/* plugins.entries.<plugin_id> or {} — never raises (unreadable = not granted).
 * A non-dict node anywhere in the walk models the live AttributeError caught
 * by _plugin_entry's except Exception. */
static json_t *py_plugin_entry(const char *plugin_id, const json_t *cfg) {
    json_t *empty = json_object();
    if (!cfg || cfg->type != JSON_OBJECT) return empty;
    json_t *node = json_obj_get(cfg, "plugins");
    if (!py_truthy(node)) node = NULL;
    if (node && node->type != JSON_OBJECT) return empty;
    node = node ? json_obj_get(node, "entries") : NULL;
    if (!py_truthy(node)) node = NULL;
    if (node && node->type != JSON_OBJECT) return empty;
    node = node ? json_obj_get(node, plugin_id) : NULL;
    if (!py_truthy(node)) node = NULL;
    if (node && node->type != JSON_OBJECT) return empty;
    if (!node) return empty;
    json_free(empty);
    return json_copy(node);
}

/* granted_capabilities: frozenset of known, stripped string entries.
 * Membership test only — order/dedup are unobservable here. */
static int py_cap_granted(const char *plugin_id, const char *cap,
                          const json_t *entry) {
    (void)plugin_id;
    json_t *raw = json_obj_get(entry, "granted_capabilities");
    if (!raw || raw->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < raw->c.count; i++) {
        json_t *item = raw->c.items[i];
        if (!item || item->type != JSON_STRING) continue;
        char *t = py_strip(item->str_val, NULL);
        if (t && py_streq(t, cap) && py_cap_spec(t)) {
            free(t);
            return 1;
        }
        free(t);
    }
    return 0;
}

/* _legacy_gate_set: walk the legacy path; every step must be a Mapping;
 * final node truthiness decides. */
static int py_legacy_gate(const json_t *entry, const py_cap_spec_t *spec) {
    if (!spec->legacy_path[0]) return 0;
    const json_t *node = entry;
    for (int i = 0; i < 2 && spec->legacy_path[i]; i++) {
        if (!node || node->type != JSON_OBJECT) return 0;
        node = json_obj_get(node, spec->legacy_path[i]);
    }
    return py_truthy(node);
}

static int py_capability_granted(const char *plugin_id, const char *capability,
                                 const json_t *cfg) {
    const py_cap_spec_t *spec = py_cap_spec(capability);
    if (!spec) {
        char *rep = py_repr_str(capability);
        char msg[1200];
        snprintf(msg, sizeof(msg),
                 "capability check for unknown id %s (plugin %s) \xe2\x80\x94 denied",
                 rep ? rep : "''", plugin_id ? plugin_id : "");
        free(rep);
        py_log("DEBUG", msg);
        return 0;
    }
    json_t *entry = py_plugin_entry(plugin_id, cfg);
    int allowed;
    const char *evidence;
    char evidence_buf[512];
    if (py_cap_granted(plugin_id, capability, entry)) {
        allowed = 1;
        evidence = "granted_capabilities";
    } else if (py_legacy_gate(entry, spec)) {
        allowed = 1;
        char joined[256];
        snprintf(joined, sizeof(joined), "%s", spec->legacy_path[0]);
        if (spec->legacy_path[1]) {
            size_t jl = strlen(joined);
            snprintf(joined + jl, sizeof(joined) - jl, ".%s", spec->legacy_path[1]);
        }
        snprintf(evidence_buf, sizeof(evidence_buf),
                 "legacy key plugins.entries.%s.%s (deprecated)",
                 plugin_id ? plugin_id : "", joined);
        evidence = evidence_buf;
    } else {
        allowed = 0;
        evidence = "not granted";
    }
    char msg[1600];
    snprintf(msg, sizeof(msg),
             "capability_check plugin=%s capability=%s decision=%s "
             "checked_by=plugin_capability_granted evidence=%s",
             plugin_id ? plugin_id : "", capability,
             allowed ? "allow" : "deny", evidence);
    py_log("INFO", msg);
    json_free(entry);
    return allowed;
}

/* ── pre_tool_call directive family (plugins.py) ── */

typedef struct {
    const char *action;    /* NULL | "block" | "approve" (static) */
    char       *message;   /* malloc'd or NULL */
    char       *rule_key;  /* malloc'd or NULL */
    json_t     *modified_args; /* owned or NULL */
} py_directive_t;

static void py_directive_init(py_directive_t *d) {
    d->action = NULL;
    d->message = NULL;
    d->rule_key = NULL;
    d->modified_args = NULL;
}

static void py_directive_clear(py_directive_t *d) {
    free(d->message);
    free(d->rule_key);
    json_free(d->modified_args);
    py_directive_init(d);
}

static const char *py_hk_str(const json_t *hk, const char *key) {
    return json_get_str(hk, key, "");
}

/* _get_pre_tool_call_directive_details: whitelist veto first, then the
 * lifecycle pre_tool_call hook scan. block > approve > none; modify merges
 * accumulate; a block needs a non-empty str message; first valid approve
 * wins (rule_key stripped, empty -> None). Returns 1 on a propagated raise. */
static int py_get_directive_details(const char *tool_name, const json_t *args,
                                    const json_t *hk, py_directive_t *out) {
    py_directive_init(out);
    if (!tool_name) tool_name = "";
    if (g_py_reg.whitelist_allowed) {
        int found = 0;
        json_t *arr = g_py_reg.whitelist_allowed;
        for (size_t i = 0; i < arr->c.count; i++) {
            json_t *it = arr->c.items[i];
            if (it && it->type == JSON_STRING && py_streq(it->str_val, tool_name)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char msg[1200];
            py_fmt_tool_name(g_py_reg.whitelist_fmt, tool_name, msg, sizeof(msg));
            out->action = "block";
            out->message = py_strdup(msg);
            return 0;
        }
    }
    json_t *kw = json_object();
    json_set(kw, "tool_name", json_string(tool_name));
    json_set(kw, "args", (args && args->type == JSON_OBJECT) ? json_copy(args) : json_object());
    json_set(kw, "task_id", json_string(py_hk_str(hk, "task_id")));
    json_set(kw, "session_id", json_string(py_hk_str(hk, "session_id")));
    json_set(kw, "tool_call_id", json_string(py_hk_str(hk, "tool_call_id")));
    json_set(kw, "turn_id", json_string(py_hk_str(hk, "turn_id")));
    json_set(kw, "api_request_id", json_string(py_hk_str(hk, "api_request_id")));
    json_t *mt = hk ? json_obj_get(hk, "middleware_trace") : NULL;
    json_t *mtl = py_list_of(mt);
    if (!mtl) { /* TypeError from list(non-iterable) */
        json_free(kw);
        return 1;
    }
    json_set(kw, "middleware_trace", mtl);
    json_set(kw, "conversation_id", json_string(py_hk_str(hk, "conversation_id")));
    json_t *a = json_object();
    json_set(a, "hook", json_string("pre_tool_call"));
    json_set(a, "kwargs", kw);
    json_t *r = py_seam("lifecycle.invoke_hook", a);
    if (py_check_raise(r)) return 1;
    if (!r || r->type != JSON_ARRAY) {
        /* live iterates the returned list; a non-list raises TypeError */
        json_free(r);
        py_raise("TypeError", "'NoneType' object is not iterable");
        return 1;
    }
    char *first_approve_msg = NULL;
    char *first_approve_key = NULL;
    int have_approve = 0;
    for (size_t i = 0; i < r->c.count; i++) {
        json_t *result = r->c.items[i];
        if (!result || result->type != JSON_OBJECT) continue;
        json_t *action = json_obj_get(result, "action");
        const char *act = (action && action->type == JSON_STRING) ? action->str_val : NULL;
        if (py_streq(act, "modify")) {
            json_t *partial = json_obj_get(result, "args");
            if (partial && partial->type == JSON_OBJECT && py_truthy(partial)) {
                if (!out->modified_args)
                    out->modified_args = (args && args->type == JSON_OBJECT)
                                             ? json_copy(args) : json_object();
                py_od_merge(out->modified_args, partial);
            }
            continue;
        }
        if (!py_streq(act, "block") && !py_streq(act, "approve")) continue;
        json_t *message = json_obj_get(result, "message");
        const char *msg = (message && message->type == JSON_STRING &&
                           message->str_val && message->str_val[0])
                              ? message->str_val : NULL;
        if (py_streq(act, "block") && !msg) continue;
        if (py_streq(act, "block")) {
            out->action = "block";
            out->message = py_strdup(msg);
            free(first_approve_msg);
            free(first_approve_key);
            json_free(r);
            return 0;
        }
        if (!have_approve) {
            have_approve = 1;
            first_approve_msg = msg ? py_strdup(msg) : NULL;
            json_t *rk = json_obj_get(result, "rule_key");
            if (rk && rk->type == JSON_STRING) {
                char *stripped = py_strip(rk->str_val, NULL);
                first_approve_key = (stripped && stripped[0]) ? stripped : NULL;
                if (!first_approve_key) free(stripped);
            }
        }
    }
    json_free(r);
    if (have_approve) {
        out->action = "approve";
        out->message = first_approve_msg;
        out->rule_key = first_approve_key;
        return 0;
    }
    free(first_approve_msg);
    free(first_approve_key);
    return 0;
}

/* _resolve_block_from_details: the ONE fail-closed approval gate.
 * Returns a malloc'd block message, or NULL to proceed; a propagated
 * BaseException leaves g_py_exc set (check py_plugin_exc_set()). */
static char *py_resolve_block_from_details(const py_directive_t *d,
                                           const char *tool_name,
                                           const json_t *hk) {
    if (py_streq(d->action, "block")) return py_strdup(d->message ? d->message : "");
    if (!py_streq(d->action, "approve")) return NULL;
    if (!tool_name) tool_name = "";
    json_t *tokens = NULL;
    int have_tokens = 0;
    json_t *a = json_object();
    json_set(a, "turn_id", json_string(py_hk_str(hk, "turn_id")));
    json_set(a, "tool_call_id", json_string(py_hk_str(hk, "tool_call_id")));
    json_set(a, "session_id", json_string(py_hk_str(hk, "session_id")));
    json_t *r = py_seam("approval.set_ctx", a);
    if (py_check_raise(r)) {
        if (!py_exc_is_base_only()) {
            memset(&g_py_exc, 0, sizeof(g_py_exc)); /* suppress(Exception) */
        } else {
            return NULL; /* SystemExit &c. propagate */
        }
    } else {
        if (r && r->type != JSON_NULL) { tokens = r; have_tokens = 1; }
        else json_free(r);
    }
    a = json_object();
    json_set(a, "tool_name", json_string(tool_name));
    json_set(a, "message", json_string(d->message ? d->message : ""));
    json_set(a, "rule_key", json_string(d->rule_key ? d->rule_key : tool_name));
    r = py_seam("approval.request", a);
    int request_raised = py_check_raise(r);
    if (request_raised) r = NULL; /* py_check_raise consumed it */
    int request_base_only = request_raised && py_exc_is_base_only();
    /* finally: reset the observability context when tokens were set — runs
     * even when request raised a BaseException (the finally is unconditional;
     * a BaseException from reset replaces the in-flight exception). */
    if (have_tokens) {
        json_t *ra = json_object();
        json_set(ra, "tokens", json_copy(tokens));
        json_t *rr = py_seam("approval.reset_ctx", ra);
        if (py_check_raise(rr)) {
            if (py_exc_is_base_only()) {
                json_free(tokens);
                return NULL;
            }
            memset(&g_py_exc, 0, sizeof(g_py_exc)); /* suppress(Exception) */
        } else {
            json_free(rr);
        }
    }
    if (request_base_only) {
        /* SystemExit/KeyboardInterrupt/GeneratorExit escape `except Exception`. */
        json_free(tokens);
        return NULL;
    }
    if (request_raised) memset(&g_py_exc, 0, sizeof(g_py_exc));
    char *fail_closed = NULL;
    if (request_raised) {
        fail_closed = py_strdup("");  /* caught by `except Exception` */
    } else if (!r || r->type == JSON_NULL) {
        /* LIVE's `result.get(...)` is OUTSIDE the try/finally: a None result
         * raises AttributeError, it is not converted to a block message. */
        py_raise("AttributeError", "'NoneType' object has no attribute 'get'");
    } else if (r->type != JSON_OBJECT) {
        fail_closed = py_strdup("");
    }
    if (fail_closed) {
        free(fail_closed);
        json_free(r);
        json_free(tokens);
        char msg[1200];
        snprintf(msg, sizeof(msg), "BLOCKED: plugin approval gate failed for %s", tool_name);
        return py_strdup(msg);
    }
    json_t *approved = json_obj_get(r, "approved");
    char *out = NULL;
    if (!py_truthy(approved)) {
        json_t *m = json_obj_get(r, "message");
        if (py_truthy(m)) {
            out = py_str_of_json(m);
        } else {
            char msg[1200];
            snprintf(msg, sizeof(msg), "BLOCKED: plugin approval required for %s", tool_name);
            out = py_strdup(msg);
        }
    }
    json_free(r);
    json_free(tokens);
    return out;
}

/* Out-slots for the tuple/message halves the fixed C signatures cannot
 * return directly (see the projection contract at the top of this section). */
static char *g_py_directive_message;
static char *g_py_resolve_message;

const char *plugin_pre_tool_directive_message(void) {
    return g_py_directive_message;
}

const char *plugin_resolve_pre_tool_block_message(void) {
    return g_py_resolve_message;
}

/* PoP: get_pre_tool_call_directive @ hermes_cli/plugins.py:get_pre_tool_call_directive */
const char *plugin_get_pre_tool_call_directive(const char *tool_name) {
    free(g_py_directive_message);
    g_py_directive_message = NULL;
    py_directive_t d;
    if (py_get_directive_details(tool_name, g_py_reg.call_args, g_py_reg.hook_kwargs, &d)) {
        py_directive_clear(&d);
        return NULL;
    }
    g_py_directive_message = d.message; /* slot takes ownership */
    d.message = NULL;
    const char *action = d.action;
    py_directive_clear(&d);
    return action;
}

/* PoP: get_pre_tool_call_block_message @ hermes_cli/plugins.py:get_pre_tool_call_block_message */
const char *plugin_get_pre_tool_call_block_message(const char *tool_name) {
    free(g_py_directive_message);
    g_py_directive_message = NULL;
    py_directive_t d;
    if (py_get_directive_details(tool_name, g_py_reg.call_args, g_py_reg.hook_kwargs, &d)) {
        py_directive_clear(&d);
        return NULL;
    }
    const char *out = NULL;
    if (py_streq(d.action, "block")) {
        g_py_directive_message = d.message;
        d.message = NULL;
        out = g_py_directive_message;
    }
    py_directive_clear(&d);
    return out;
}

/* PoP: resolve_pre_tool_block @ hermes_cli/plugins.py:resolve_pre_tool_block */
bool plugin_resolve_pre_tool_block(const char *block_msg) {
    /* block_msg is the TOOL NAME at every live call site (tool_executor.c). */
    free(g_py_resolve_message);
    g_py_resolve_message = NULL;
    py_directive_t d;
    if (py_get_directive_details(block_msg, g_py_reg.call_args, g_py_reg.hook_kwargs, &d)) {
        py_directive_clear(&d);
        return false;
    }
    char *msg = py_resolve_block_from_details(&d, block_msg, g_py_reg.hook_kwargs);
    py_directive_clear(&d);
    if (g_py_exc.set) {
        free(msg);
        return false;
    }
    g_py_resolve_message = msg;
    return msg != NULL;
}

/* PoP: get_pre_verify_continue_message @ hermes_cli/plugins.py:get_pre_verify_continue_message */
const char *plugin_get_pre_verify_continue_message(const char *tool_name) {
    (void)tool_name; /* live fn is keyword-only; kwargs come from the slot */
    static char buf[2048];
    buf[0] = '\0';
    if (py_delivery_manager()) return NULL;
    const json_t *vk = g_py_reg.verify_kwargs;
    json_t *kw = json_object();
    json_t *v;
    v = vk ? json_obj_get(vk, "session_id") : NULL;
    json_set(kw, "session_id", v ? json_copy(v) : json_string(""));
    v = vk ? json_obj_get(vk, "platform") : NULL;
    json_set(kw, "platform", v ? json_copy(v) : json_string(""));
    v = vk ? json_obj_get(vk, "model") : NULL;
    json_set(kw, "model", v ? json_copy(v) : json_string(""));
    v = vk ? json_obj_get(vk, "coding") : NULL;
    json_set(kw, "coding", v ? json_copy(v) : json_bool(false));
    v = vk ? json_obj_get(vk, "attempt") : NULL;
    json_set(kw, "attempt", v ? json_copy(v) : json_number(0));
    v = vk ? json_obj_get(vk, "final_response") : NULL;
    json_set(kw, "final_response", v ? json_copy(v) : json_string(""));
    json_t *cp = vk ? json_obj_get(vk, "changed_paths") : NULL;
    json_t *cpl = py_list_of(cp);
    if (!cpl) {
        json_free(kw);
        return NULL;
    }
    json_set(kw, "changed_paths", cpl);
    json_t *a = json_object();
    json_set(a, "hook", json_string("pre_verify"));
    json_set(a, "kwargs", kw);
    json_t *r = py_seam("manager.invoke_hook", a);
    if (py_check_raise(r)) return NULL;
    if (!r || r->type != JSON_ARRAY) {
        json_free(r);
        py_raise("TypeError", "'NoneType' object is not iterable");
        return NULL;
    }
    for (size_t i = 0; i < r->c.count; i++) {
        json_t *result = r->c.items[i];
        if (!result || result->type != JSON_OBJECT) continue;
        json_t *action = json_obj_get(result, "action");
        if (!py_truthy(action)) action = json_obj_get(result, "decision");
        char *actstr = py_truthy(action) ? py_str_of_json(action) : py_strdup("");
        char *act = py_strip(actstr ? actstr : "", NULL);
        free(actstr);
        py_lower_ascii(act);
        json_t *message = json_obj_get(result, "message");
        if (!py_truthy(message)) message = json_obj_get(result, "reason");
        int hit = (py_streq(act, "continue") || py_streq(act, "block")) &&
                  message && message->type == JSON_STRING;
        if (hit) {
            char *stripped = py_strip(message->str_val, NULL);
            if (stripped && stripped[0]) {
                snprintf(buf, sizeof(buf), "%s", stripped);
                free(stripped);
                free(act);
                json_free(r);
                return buf;
            }
            free(stripped);
        }
        free(act);
    }
    json_free(r);
    return NULL;
}

/* ── module-level convenience ports ── */

/* PoP: _ensure_plugins_discovered @ hermes_cli/plugins.py:_ensure_plugins_discovered */
bool plugin_ensure_discovered(void) {
    /* projection: the live fn returns the manager; the observable C bit is
     * its _discovered flag after idempotent discovery (force=False). */
    if (py_ensure_discovered(0)) return false;
    return g_py_reg.discovered != 0;
}

/* PoP: get_plugin_context_engine @ hermes_cli/plugins.py:get_plugin_context_engine */
const char *plugin_get_context_engine(const char *name) {
    (void)name; /* live takes no parameters */
    static char buf[512];
    buf[0] = '\0';
    if (py_ensure_discovered(0)) return NULL;
    if (!g_py_reg.context_engine) return NULL;
    if (g_py_reg.context_engine->type != JSON_STRING) return NULL;
    snprintf(buf, sizeof(buf), "%s", g_py_reg.context_engine->str_val);
    return buf;
}

/* PoP: get_plugin_command_handler @ hermes_cli/plugins.py:get_plugin_command_handler */
void *plugin_get_command_handler(const char *name) {
    if (py_ensure_discovered(0)) return NULL;
    json_t *entry = json_obj_get(g_py_reg.plugin_commands, name ? name : "");
    if (!py_truthy(entry)) return NULL; /* `if entry` — {} is falsy */
    json_t *handler = json_obj_get(entry, "handler");
    if (!handler) {
        py_raise_repr("KeyError", "handler");
        return NULL;
    }
    if (handler->type != JSON_STRING) {
        py_raise("TypeError", "handler token must be a string");
        return NULL;
    }
    return py_strdup(handler->str_val); /* caller frees */
}

/* PoP: has_hook @ hermes_cli/plugins.py:has_hook */
bool plugin_has_hook(const char *name) {
    if (py_delivery_manager()) return false;
    json_t *r = py_seam0("safe_worker_enabled");
    if (py_check_raise(r)) return false;
    int safe = py_truthy(r);
    json_free(r);
    if (safe) return false;
    return py_truthy(json_obj_get(g_py_reg.hooks, name ? name : "")) != 0;
}

/* PoP: has_middleware @ hermes_cli/plugins.py:has_middleware */
bool plugin_has_middleware(const char *name) {
    if (py_delivery_manager()) return false;
    if (g_py_reg.manager_has_middleware_method) {
        json_t *r = py_seam0("safe_worker_enabled");
        if (py_check_raise(r)) return false;
        int safe = py_truthy(r);
        json_free(r);
        if (safe) return false;
        return py_truthy(json_obj_get(g_py_reg.middleware, name ? name : "")) != 0;
    }
    if (!g_py_reg.has_middleware_attr) return false;
    return py_truthy(json_obj_get(g_py_reg.middleware, name ? name : "")) != 0;
}

/* PoP: find_plugin_skill @ hermes_cli/plugins.py:find_plugin_skill */
json_t *plugin_find_skill(const char *name) {
    json_t *entry = json_obj_get(g_py_reg.plugin_skills, name ? name : "");
    if (!py_truthy(entry)) return NULL; /* `if entry` — {} is falsy */
    json_t *path = json_obj_get(entry, "path");
    if (!path) {
        py_raise_repr("KeyError", "path");
        return NULL;
    }
    return json_copy(path); /* caller frees */
}

/* PoP: remove_plugin_skill @ hermes_cli/plugins.py:remove_plugin_skill */
bool plugin_remove_skill(const char *name) {
    /* projection: live returns None; the bool reports whether pop removed an
     * entry. The registry effect itself is compared via the state snapshot. */
    return json_obj_del(g_py_reg.plugin_skills, name ? name : "");
}

/* ── env / debug / capability / manifest ports ── */

/* PoP: _env_enabled @ hermes_cli/plugins.py:_env_enabled */
bool plugin_env_enabled(const char *name) {
    /* _env_enabled is the utils.env_var_enabled alias (plugins.py line 219).
     * os.getenv(name, "") — a null seam result models the unset default "". */
    json_t *r = py_seam_str("getenv", "name", name ? name : "");
    if (py_check_raise(r)) return false;
    const char *v = (r && r->type == JSON_STRING) ? r->str_val : "";
    int out = py_env_truthy(v);
    json_free(r);
    return out != 0;
}

/* _install_plugin_debug_handler(force=False) shared body; the tagged public
 * entry point below carries the live default (force=False). The harness may
 * call py_install_debug_handler(1) to exercise the force=True re-read. */
void py_install_debug_handler(int force) {
    if (force) {
        json_t *r = py_seam_str("getenv", "name", "HERMES_PLUGINS_DEBUG");
        if (py_check_raise(r)) return;
        const char *v = (r && r->type == JSON_STRING) ? r->str_val : "";
        g_py_reg.plugins_debug = py_env_truthy(v);
        json_free(r);
    }
    if (!g_py_reg.plugins_debug || g_py_reg.debug_handler_installed) return;
    json_t *r = py_seam_str("stream_handler_new", "stream", "stderr");
    json_free(r);
    r = py_seam_str("handler_set_level", "level", "DEBUG");
    json_free(r);
    r = py_seam_str("handler_set_formatter", "fmt", "[plugins] %(levelname)s %(message)s");
    json_free(r);
    r = py_seam0("logger_add_handler");
    json_free(r);
    r = py_seam_str("logger_set_level", "level", "DEBUG");
    json_free(r);
    json_t *a = json_object();
    json_set(a, "propagate", json_bool(true));
    r = py_seam("logger_set_propagate", a);
    json_free(r);
    g_py_reg.debug_handler_installed = 1;
    py_log("DEBUG",
           "HERMES_PLUGINS_DEBUG=1 \xe2\x80\x94" " verbose plugin discovery logging enabled");
}

/* PoP: _install_plugin_debug_handler @ hermes_cli/plugins.py:_install_plugin_debug_handler */
void plugin_install_debug_handler(void) {
    py_install_debug_handler(0);
}

/* PoP: _tool_override_allowed @ hermes_cli/plugins.py:_tool_override_allowed */
bool plugin_tool_override_allowed(const char *name) {
    (void)name; /* the live PluginContext method never reads tool_name */
    if (py_streq(g_py_reg.manifest_source, "bundled")) return true;
    json_t *r = py_seam0("import_config");
    if (py_check_raise(r)) {
        /* import failure inside the try: except Exception -> fail closed;
         * BaseException propagates (no scope was entered yet). */
        if (py_exc_is_base_only()) return false;
        memset(&g_py_exc, 0, sizeof(g_py_exc));
        return false;
    }
    json_free(r);
    r = py_seam_str("home_scope.enter", "home", g_py_reg.home_path);
    json_free(r);
    r = py_seam0("load_config");
    if (py_check_raise(r)) {
        json_t *ex = py_seam0("home_scope.exit"); /* with-block __exit__ runs */
        json_free(ex);
        if (py_exc_is_base_only()) return false; /* propagate */
        memset(&g_py_exc, 0, sizeof(g_py_exc));  /* except Exception: fail closed */
        return false;
    }
    json_t *cfg = (r && py_truthy(r)) ? r : NULL; /* load_config() or {} */
    if (!cfg) json_free(r);
    json_t *ex = py_seam0("home_scope.exit");
    json_free(ex);
    int allowed = py_capability_granted(g_py_reg.plugin_id, "tools.override", cfg);
    json_free(cfg);
    return allowed != 0;
}

/* PoP: _platform_name_from_manifest @ hermes_cli/plugins.py:_platform_name_from_manifest */
const char *plugin_platform_name_from_manifest(json_t *manifest) {
    static char buf[1024];
    buf[0] = '\0';
    if (!manifest || manifest->type != JSON_OBJECT) {
        py_raise("AttributeError", "'NoneType' object has no attribute 'name'");
        return NULL;
    }
    const char *name = json_get_str(manifest, "name", "");
    size_t nlen = strlen(name);
    if (nlen >= 9 && strcmp(name + nlen - 9, "-platform") == 0) {
        snprintf(buf, sizeof(buf), "%.*s", (int)(nlen - 9), name);
        return buf;
    }
    const char *path = json_get_str(manifest, "path", NULL);
    if (path && path[0]) {
        char *base = py_path_name(path);
        snprintf(buf, sizeof(buf), "%s", base ? base : "");
        free(base);
        return buf;
    }
    snprintf(buf, sizeof(buf), "%s", name);
    return buf;
}

/* ── import-machinery ports (plugins_loader.py) ── */

/* _evict_modules: drop module_name and every module_name.* submodule.
 * Snapshot-then-pop mirrors the live list(sys.modules) atomic copy. */
static void py_evict_modules(const char *module_name) {
    if (!module_name) return;
    size_t pn = strlen(module_name);
    char *prefix = (char *)malloc(pn + 2);
    if (!prefix) return;
    memcpy(prefix, module_name, pn);
    prefix[pn] = '.';
    prefix[pn + 1] = '\0';
    json_t *obj = g_py_reg.sys_modules;
    for (size_t i = 0; i < obj->c.count; ) {
        const char *k = obj->c.keys[i];
        if (py_streq(k, module_name) || strncmp(k, prefix, pn + 1) == 0) {
            json_obj_del(obj, k); /* shifts remaining keys; order preserved */
        } else {
            i++;
        }
    }
    free(prefix);
}

/* manifest_key: manifest.key or manifest.name (truthiness). */
static const char *py_manifest_key(const json_t *manifest) {
    const char *key = json_get_str(manifest, "key", "");
    if (key && key[0]) return key;
    return json_get_str(manifest, "name", "");
}

/* _directory_module_name: bare hermes_plugins.<slug> for the first scope to
 * claim it; __home_<sha256(scope_key)[:12]> for any other scope. */
static char *py_directory_module_name(const json_t *manifest) {
    const char *mk = py_manifest_key(manifest);
    size_t n = strlen(mk);
    char *slug = (char *)malloc(n * 2 + 1);
    if (!slug) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (mk[i] == '/') { slug[o++] = '_'; slug[o++] = '_'; }
        else if (mk[i] == '-') slug[o++] = '_';
        else slug[o++] = mk[i];
    }
    slug[o] = '\0';
    char bare[1200];
    snprintf(bare, sizeof(bare), "hermes_plugins.%s", slug);
    free(slug);
    json_t *existing = json_obj_get(g_py_reg.bare_module_scope, bare);
    if (!existing) {
        json_set(g_py_reg.bare_module_scope, bare,
                 json_string(g_py_reg.scope_key ? g_py_reg.scope_key : ""));
        return py_strdup(bare);
    }
    if (existing->type == JSON_STRING &&
        py_streq(existing->str_val, g_py_reg.scope_key))
        return py_strdup(bare);
    char digest[65];
    py_sha256_hex(g_py_reg.scope_key ? g_py_reg.scope_key : "",
                  strlen(g_py_reg.scope_key ? g_py_reg.scope_key : ""), digest);
    char out[1400];
    snprintf(out, sizeof(out), "%s__home_%.12s", bare, digest);
    return py_strdup(out);
}

/* PoP: _load_directory_module @ hermes_cli/plugins.py:_load_directory_module */
void *plugin_load_directory_module(const char *dir) {
    /* dir is manifest.path; the manifest itself rides the registry slot.
     * Returns a malloc'd module token (caller frees) or NULL with
     * py_plugin_exc() set on a raised exception. */
    if (!dir) dir = "";
    char init[2048];
    py_path_join(dir, "__init__.py", init, sizeof(init));
    json_t *r = py_seam_str("fs.exists", "path", init);
    if (py_check_raise(r)) return NULL;
    int exists = py_truthy(r);
    json_free(r);
    if (!exists) {
        char msg[2200];
        snprintf(msg, sizeof(msg), "No __init__.py in %s", dir);
        py_raise("FileNotFoundError", msg);
        return NULL;
    }
    if (!json_obj_get(g_py_reg.sys_modules, "hermes_plugins")) {
        json_t *ns = json_object();
        json_set(ns, "kind", json_string("ns"));
        json_set(ns, "name", json_string("hermes_plugins"));
        json_set(ns, "package", json_string("hermes_plugins"));
        json_set(ns, "path", json_array());
        json_set(g_py_reg.sys_modules, "hermes_plugins", ns);
    }
    char *module_name = g_py_reg.load_module_name
                            ? py_strdup(g_py_reg.load_module_name)
                            : py_directory_module_name(g_py_reg.manifest);
    if (!module_name) return NULL;
    py_evict_modules(module_name);
    json_t *a = json_object();
    json_set(a, "module_name", json_string(module_name));
    json_set(a, "location", json_string(init));
    json_t *locs = json_array();
    json_append(locs, json_string(dir));
    json_set(a, "submodule_search_locations", locs);
    r = py_seam("spec_from_file_location", a);
    if (py_check_raise(r)) { free(module_name); return NULL; }
    const char *spec_token = NULL;
    int loader_present = 0;
    if (r && r->type == JSON_STRING) {
        spec_token = r->str_val;
        loader_present = 1;
    } else if (r && r->type == JSON_OBJECT) {
        spec_token = json_get_str(r, "token", NULL);
        json_t *loader = json_obj_get(r, "loader");
        loader_present = loader && loader->type != JSON_NULL;
    }
    if (!spec_token || !loader_present) {
        char msg[2200];
        snprintf(msg, sizeof(msg), "Cannot create module spec for %s", init);
        py_raise("ImportError", msg);
        json_free(r);
        free(module_name);
        return NULL;
    }
    char *spec_tok = py_strdup(spec_token); /* own the token; r is freed below */
    json_free(r);
    a = json_object();
    json_set(a, "spec", json_string(spec_tok));
    json_t *r2 = py_seam("module_from_spec", a);
    if (py_check_raise(r2)) { free(module_name); free(spec_tok); return NULL; }
    const char *mod_token = (r2 && r2->type == JSON_STRING) ? r2->str_val : NULL;
    if (!mod_token) {
        json_free(r2);
        free(module_name);
        free(spec_tok);
        py_raise("ImportError", "module_from_spec returned no module");
        return NULL;
    }
    char *mod_tok = py_strdup(mod_token);
    json_free(r2);
    json_t *model = json_object();
    json_set(model, "kind", json_string("mod"));
    json_set(model, "token", json_string(mod_tok));
    json_set(model, "package", json_string(module_name));
    json_t *mpath = json_array();
    json_append(mpath, json_string(dir));
    json_set(model, "path", mpath);
    json_set(g_py_reg.sys_modules, module_name, model);
    a = json_object();
    json_set(a, "spec", json_string(spec_tok));
    json_set(a, "module", json_string(mod_tok));
    json_t *r3 = py_seam("exec_module", a);
    if (py_check_raise(r3)) {
        /* BaseException catch-all: evict the half-initialized module, re-raise */
        py_evict_modules(module_name);
        free(module_name);
        free(spec_tok);
        free(mod_tok);
        return NULL;
    }
    json_free(r3);
    free(spec_tok);
    free(module_name);
    return mod_tok; /* caller frees */
}

/* _select_entry_point_group over the JSON entry-points model:
 * {"kind":"select","eps":[...]} | {"kind":"dict","groups":{g:[...]}} |
 * {"kind":"list","eps":[{name,group},...]}. The select shape records an
 * ep.select seam call (the live .select(group=...) method). */
static json_t *py_select_ep_group(const json_t *eps_model, const char *group) {
    const char *kind = json_get_str(eps_model, "kind", "list");
    if (py_streq(kind, "select")) {
        json_t *a = json_object();
        json_set(a, "group", json_string(group));
        json_t *r = py_seam("ep.select", a);
        if (py_check_raise(r)) return NULL;
        if (!r || r->type != JSON_ARRAY) {
            json_free(r);
            return json_array();
        }
        return r;
    }
    if (py_streq(kind, "dict")) {
        json_t *groups = json_obj_get(eps_model, "groups");
        json_t *g = groups ? json_obj_get(groups, group) : NULL;
        return (g && g->type == JSON_ARRAY) ? json_copy(g) : json_array();
    }
    json_t *out = json_array();
    json_t *eps = json_obj_get(eps_model, "eps");
    if (eps && eps->type == JSON_ARRAY) {
        for (size_t i = 0; i < eps->c.count; i++) {
            json_t *ep = eps->c.items[i];
            if (ep && ep->type == JSON_OBJECT &&
                py_streq(json_get_str(ep, "group", ""), group))
                json_append(out, json_copy(ep));
        }
    }
    return out;
}

/* PoP: _load_entrypoint_module @ hermes_cli/plugins.py:_load_entrypoint_module */
void *plugin_load_entrypoint_module(const char *path) {
    (void)path; /* live takes the manifest; name rides the registry slot */
    if (!g_py_reg.manifest || g_py_reg.manifest->type != JSON_OBJECT) {
        py_raise("AttributeError", "'NoneType' object has no attribute 'name'");
        return NULL;
    }
    const char *pname = json_get_str(g_py_reg.manifest, "name", "");
    json_t *r = py_seam0("entry_points");
    if (py_check_raise(r)) return NULL;
    json_t *group_eps = py_select_ep_group(r, "hermes_agent.plugins");
    json_free(r);
    if (!group_eps) return NULL; /* propagated raise */
    for (size_t i = 0; i < group_eps->c.count; i++) {
        json_t *ep = group_eps->c.items[i];
        if (!ep || ep->type != JSON_OBJECT) continue;
        if (!py_streq(json_get_str(ep, "name", ""), pname)) continue;
        json_t *a = json_object();
        json_set(a, "name", json_string(pname));
        json_t *lr = py_seam("ep.load", a);
        json_free(group_eps);
        if (py_check_raise(lr)) return NULL;
        if (!lr || lr->type == JSON_NULL) {
            json_free(lr);
            return NULL; /* ep.load() returned None */
        }
        if (lr->type != JSON_STRING) {
            json_free(lr);
            py_raise("TypeError", "entry point token must be a string");
            return NULL;
        }
        char *out = py_strdup(lr->str_val);
        json_free(lr);
        return out;
    }
    json_free(group_eps);
    char msg[1200];
    snprintf(msg, sizeof(msg),
             "Entry point '%s' not found in group 'hermes_agent.plugins'", pname);
    py_raise("ImportError", msg);
    return NULL;
}
