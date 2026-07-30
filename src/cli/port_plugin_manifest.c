/*
 * port_plugin_manifest.c — pure data-model port of plugins.py:PluginManifest
 *
 * Faithful C port of _parse_manifest(): derive name/key, validate+auto-coerce
 * kind, normalize requires_env. The YAML->JSON conversion and the __init__.py
 * filesystem scan are injected (detector callback), so this module is pure.
 */

#include "plugin_manifest.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "json.h"

/* ── validation constants (mirrors _VALID_PLUGIN_KINDS) ─────────────────── */
static const char *VALID_KINDS[] = {
    "standalone", "backend", "exclusive", "platform", "model-provider", NULL
};

static int set_str(char **dst, const char *src) {
    free(*dst);
    *dst = src ? strdup(src) : NULL;
    return *dst ? 0 : (src ? -1 : 0);
}

/* PoP: plugin_manifest_kind_is_valid @ hermes_cli/plugins.py:_VALID_PLUGIN_KINDS */
bool plugin_manifest_kind_is_valid(const char *kind) {
    if (!kind) return false;
    for (size_t i = 0; VALID_KINDS[i]; i++)
        if (strcmp(VALID_KINDS[i], kind) == 0) return true;
    return false;
}

struct plugin_manifest_t {
    char *name;
    char *version;
    char *description;
    char *author;
    char *source;
    char *path;
    char *kind;
    char *key;
    plugin_req_env_t *req_env;
    size_t n_req_env;
    char **provides_tools;
    size_t n_tools;
    char **provides_hooks;
    size_t n_hooks;
};

/* normalize a single requires_env element: str -> {name}; dict -> {name,extra} */
static int req_env_push(plugin_manifest_t *m, const char *name, const char *extra_json) {
    plugin_req_env_t *na = realloc(m->req_env, (m->n_req_env + 1) * sizeof(*na));
    if (!na) return -1;
    m->req_env = na;
    plugin_req_env_t *e = &m->req_env[m->n_req_env];
    e->name = name ? strdup(name) : strdup("");
    e->extra_json = extra_json ? strdup(extra_json) : NULL;
    m->n_req_env++;
    return 0;
}

static int str_push(char ***arr, size_t *n, const char *s) {
    char **na = realloc(*arr, (*n + 1) * sizeof(*na));
    if (!na) return -1;
    *arr = na;
    (*arr)[*n] = s ? strdup(s) : strdup("");
    (*n)++;
    return 0;
}

/* PoP: plugin_manifest_parse @ hermes_cli/plugins.py:_parse_manifest */
plugin_manifest_t *plugin_manifest_parse(const char *json,
                                         const char *plugin_dir_name,
                                         const char *prefix,
                                         const char *source,
                                         const plugin_kind_detector_t detector) {
    if (!json) return NULL;
    char *err = NULL;
    json_t *doc = json_parse(json, &err);
    if (err) { free(err); return NULL; }
    if (!doc || doc->type != JSON_OBJECT) { json_free(doc); return NULL; }

    plugin_manifest_t *m = calloc(1, sizeof(*m));
    if (!m) { json_free(doc); return NULL; }

    const char *raw = json_get_str(doc, "name", NULL);
    const char *name = (raw && raw[0]) ? raw : (plugin_dir_name ? plugin_dir_name : "");
    set_str(&m->name, name);
    set_str(&m->version, json_get_str(doc, "version", ""));
    set_str(&m->description, json_get_str(doc, "description", ""));
    set_str(&m->author, json_get_str(doc, "author", ""));
    set_str(&m->source, source ? source : "");
    set_str(&m->path, NULL); /* path is host-supplied separately if needed */

    /* key derivation: prefix/dir-name when prefix, else name */
    if (prefix && prefix[0]) {
        size_t need = strlen(prefix) + 1 + strlen(plugin_dir_name ? plugin_dir_name : "") + 1;
        char *k = malloc(need);
        snprintf(k, need, "%s/%s", prefix, plugin_dir_name ? plugin_dir_name : "");
        set_str(&m->key, k);
        free(k);
    } else {
        set_str(&m->key, m->name);
    }

    /* kind: lowercase + validate */
    const char *raw_kind = json_get_str(doc, "kind", "standalone");
    char kbuf[64]; size_t j = 0;
    for (const char *p = raw_kind; *p && j + 1 < sizeof(kbuf); p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        kbuf[j++] = c;
    }
    kbuf[j] = '\0';
    if (!plugin_manifest_kind_is_valid(kbuf)) {
        /* treat as standalone (Python logs a warning; we keep it silent-pure) */
        set_str(&m->kind, "standalone");
    } else {
        set_str(&m->kind, kbuf);
    }

    /* requires_env: list of str|dict */
    json_t *re = json_obj_get(doc, "requires_env");
    if (re && re->type == JSON_ARRAY) {
        for (size_t i = 0; i < re->c.count; i++) {
            json_t *el = re->c.items[i];
            if (el->type == JSON_STRING) {
                req_env_push(m, el->str_val, NULL);
            } else if (el->type == JSON_OBJECT) {
                const char *rn = json_get_str(el, "name", NULL);
                char *ej = json_serialize(el);
                req_env_push(m, rn ? rn : "", ej ? ej : "{}");
                free(ej);
            }
        }
    }

    /* provides_tools / provides_hooks: list of str */
    json_t *pt = json_obj_get(doc, "provides_tools");
    if (pt && pt->type == JSON_ARRAY)
        for (size_t i = 0; i < pt->c.count; i++)
            if (pt->c.items[i]->type == JSON_STRING)
                str_push(&m->provides_tools, &m->n_tools, pt->c.items[i]->str_val);
    json_t *ph = json_obj_get(doc, "provides_hooks");
    if (ph && ph->type == JSON_ARRAY)
        for (size_t i = 0; i < ph->c.count; i++)
            if (ph->c.items[i]->type == JSON_STRING)
                str_push(&m->provides_hooks, &m->n_hooks, ph->c.items[i]->str_val);

    /* kind auto-coercion via injected detector (mirrors the __init__.py
     * scan in _parse_manifest, kept out of the pure module). The Python code
     * only auto-coerces when the manifest did NOT declare an explicit "kind". */
    json_t *kind_key = json_obj_get(doc, "kind");
    if (detector && m->kind && strcmp(m->kind, "standalone") == 0 && kind_key == NULL) {
        const char *coerced = detector(plugin_dir_name ? plugin_dir_name : "");
        if (coerced && plugin_manifest_kind_is_valid(coerced))
            set_str(&m->kind, coerced);
    }

    json_free(doc);
    return m;
}

void plugin_manifest_free(plugin_manifest_t *m) {
    if (!m) return;
    free(m->name); free(m->version); free(m->description);
    free(m->author); free(m->source); free(m->path); free(m->kind); free(m->key);
    for (size_t i = 0; i < m->n_req_env; i++) {
        free(m->req_env[i].name);
        free(m->req_env[i].extra_json);
    }
    free(m->req_env);
    for (size_t i = 0; i < m->n_tools; i++) free(m->provides_tools[i]);
    free(m->provides_tools);
    for (size_t i = 0; i < m->n_hooks; i++) free(m->provides_hooks[i]);
    free(m->provides_hooks);
    free(m);
}

const char *plugin_manifest_name(const plugin_manifest_t *m) { return m ? m->name : ""; }
const char *plugin_manifest_version(const plugin_manifest_t *m) { return m ? m->version : ""; }
const char *plugin_manifest_description(const plugin_manifest_t *m) { return m ? m->description : ""; }
const char *plugin_manifest_author(const plugin_manifest_t *m) { return m ? m->author : ""; }
const char *plugin_manifest_source(const plugin_manifest_t *m) { return m ? m->source : ""; }
const char *plugin_manifest_path(const plugin_manifest_t *m) { return m ? m->path : ""; }
const char *plugin_manifest_kind(const plugin_manifest_t *m) { return m ? m->kind : "standalone"; }
const char *plugin_manifest_key(const plugin_manifest_t *m) { return m ? m->key : ""; }

size_t plugin_manifest_req_env_count(const plugin_manifest_t *m) { return m ? m->n_req_env : 0; }
const plugin_req_env_t *plugin_manifest_req_env(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_req_env) return NULL;
    return &m->req_env[i];
}

size_t plugin_manifest_provides_tools_count(const plugin_manifest_t *m) { return m ? m->n_tools : 0; }
const char *plugin_manifest_provides_tool(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_tools) return NULL;
    return m->provides_tools[i];
}
size_t plugin_manifest_provides_hooks_count(const plugin_manifest_t *m) { return m ? m->n_hooks : 0; }
const char *plugin_manifest_provides_hook(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_hooks) return NULL;
    return m->provides_hooks[i];
}

char *plugin_manifest_to_json(const plugin_manifest_t *m) {
    if (!m) return strdup("{}");
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "name", json_string(m->name ? m->name : ""));
    json_set(o, "version", json_string(m->version ? m->version : ""));
    json_set(o, "description", json_string(m->description ? m->description : ""));
    json_set(o, "author", json_string(m->author ? m->author : ""));
    json_set(o, "source", json_string(m->source ? m->source : ""));
    json_set(o, "kind", json_string(m->kind ? m->kind : "standalone"));
    json_set(o, "key", json_string(m->key ? m->key : ""));
    if (m->path) json_set(o, "path", json_string(m->path));
    json_t *arr = json_array();
    for (size_t i = 0; i < m->n_req_env; i++) {
        if (m->req_env[i].extra_json) {
            char *e = NULL;
            json_t *d = json_parse(m->req_env[i].extra_json, &e);
            if (e) free(e);
            if (d) json_append(arr, d);
        } else {
            json_append(arr, json_string(m->req_env[i].name));
        }
    }
    json_set(o, "requires_env", arr);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}
