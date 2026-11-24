/*
 * plugin_validate_core_override.c -- real C11 port of the value layer of
 * upstream Hermes hermes_cli/plugin_validate_core_override.py.
 *
 * Upstream is an admission lint: a catalog plugin must not rebind Hermes core at
 * runtime (no ``AIAgent._replace_primary_openai_client = ...``, no
 * ``tui_gateway.server.handle_request``, no ``mock.patch`` of a core module, no
 * ``get_plugin_manager()._hooks["x"].append(fn)``).  It parses the plugin's
 * Python and reports the rebinds.
 *
 * PoP: w32_pluginvali_b2daaa_p1_is_test_file @ hermes_cli/plugin_validate_core_override.py:_is_test_file
 * PoP: w32_pluginvali_b2daaa_p1_should_skip @ hermes_cli/plugin_validate_core_override.py:_python_files
 * PoP: w32_pluginvali_b2daaa_p1_core_module_name @ hermes_cli/plugin_validate_core_override.py:_Analysis._core_module_name
 * PoP: w32_pluginvali_b2daaa_p1_core_roots @ hermes_cli/plugin_validate_core_override.py:_hermes_top_level
 * PoP: w32_pluginvali_b2daaa_p1_core_roots @ hermes_cli/plugin_validate_core_override.py:_Analysis.__init__
 * PoP: w32_pluginvali_b2daaa_p1_lookup_is_core @ hermes_cli/plugin_validate_core_override.py:_Analysis._lookup_is_core
 * PoP: w32_pluginvali_b2daaa_p1_is_container @ hermes_cli/plugin_validate_core_override.py:_Analysis.is_container
 * PoP: w32_pluginvali_b2daaa_p1_bound_names @ hermes_cli/plugin_validate_core_override.py:_bound_names
 * PoP: w32_pluginvali_b2daaa_p1_module_constants @ hermes_cli/plugin_validate_core_override.py:<module constants>
 * PoP: w32_pluginvali_b2daaa_p2_render @ hermes_cli/plugin_validate_core_override.py:_render
 * PoP: w32_pluginvali_b2daaa_p2_dotted @ hermes_cli/plugin_validate_core_override.py:_dotted
 * PoP: w32_pluginvali_b2daaa_p2_has_private_segment @ hermes_cli/plugin_validate_core_override.py:_has_private_segment
 * PoP: w32_pluginvali_b2daaa_p2_has_mutator_call @ hermes_cli/plugin_validate_core_override.py:_has_mutator_call
 * PoP: w32_pluginvali_b2daaa_p2_func_key @ hermes_cli/plugin_validate_core_override.py:_Analysis._func_key
 * PoP: w32_pluginvali_b2daaa_p2_helper_indices @ hermes_cli/plugin_validate_core_override.py:_Analysis.helper_indices
 * PoP: w32_pluginvali_b2daaa_p2_report @ hermes_cli/plugin_validate_core_override.py:check_core_override
 *
 * Files are seams: the plugin directory walk, ``ast.parse``, the import closure
 * (``_python_files`` / ``_runtime_files``), the helper fixpoint
 * (``_summarise_helpers``) and the per-scope taint walk all stay out.  What is
 * ported is the part that is a pure function of values: which files are in
 * scope, whether a dotted name is core, what the core-root set is, how a
 * receiver chain is named and judged, and the report message the lint ships.
 */
#include "hermes_json.h"
#include "noc_w32_pluginvali_b2daaa_p1.h"
#include "noc_w32_pluginvali_b2daaa_p2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PV_MAX_PARTS 128
#define PV_MAX_DEPTH 64

/* ---- module constants (upstream: _SKIP_DIRS / _CONTAINER_CTORS / ...) ------ */

static const char *const PV_SKIP_DIRS[] = {
    "tests", "test", "node_modules", ".git", "__pycache__", ".venv", "venv", "site-packages",
};
#define PV_SKIP_DIRS_N (sizeof(PV_SKIP_DIRS) / sizeof(PV_SKIP_DIRS[0]))

/* _CONTAINER_CTORS */
static const char *const PV_CONTAINER_CTORS[] = {"set", "dict", "list", "frozenset"};
#define PV_CONTAINER_CTORS_N (sizeof(PV_CONTAINER_CTORS) / sizeof(PV_CONTAINER_CTORS[0]))

/* _NAMESPACE_ESCAPES */
static const char *const PV_NAMESPACE_ESCAPES[] = {
    "globals", "locals", "vars", "exec", "eval", "builtins", "__builtins__", "__dict__",
    "__setattr__", "__globals__", "_getframe", "currentframe", "f_locals", "f_globals", "ctypes",
};
#define PV_NAMESPACE_ESCAPES_N (sizeof(PV_NAMESPACE_ESCAPES) / sizeof(PV_NAMESPACE_ESCAPES[0]))

/* _MUTATORS */
static const char *const PV_MUTATORS[] = {
    "append", "extend", "insert", "remove", "pop", "clear", "update", "setdefault",
    "add", "discard", "popitem",
};
#define PV_MUTATORS_N (sizeof(PV_MUTATORS) / sizeof(PV_MUTATORS[0]))

/* ---- small helpers -------------------------------------------------------- */

static int pv_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int pv_in_list(const char *s, const char *const *list, size_t n) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++)
        if (strcmp(s, list[i]) == 0) return 1;
    return 0;
}

static int pv_startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int pv_endswith(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && memcmp(s + n - m, suffix, m) == 0;
}

static char *pv_strdup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* ---- path parts (Python PurePath.parts, minus ".." folding) --------------- */

/* Split ``rel`` on both separators, dropping empty and "." segments: the last
 * part is the file name, everything before it is the directory chain. Parts
 * point into ``buf`` (a copy of rel, at least strlen(rel)+1 bytes). */
static size_t pv_split(const char *rel, char *buf, const char **parts, size_t max) {
    size_t n = 0, i = 0, len;
    if (!rel) return 0;
    len = strlen(rel);
    memcpy(buf, rel, len + 1);
    while (i < len) {
        size_t start;
        while (i < len && (buf[i] == '/' || buf[i] == '\\')) i++;
        if (i >= len) break;
        start = i;
        while (i < len && buf[i] != '/' && buf[i] != '\\') i++;
        if (i < len) {
            buf[i] = '\0';
            i++;
        }
        if (buf[start] == '\0') continue;
        if (strcmp(buf + start, ".") == 0) continue;
        if (n < max) parts[n++] = buf + start;
    }
    return n;
}

/* The last path component, both separators understood ("a/b/" -> "b"). */
static char *pv_basename(const char *rel) {
    char *buf, *name;
    const char *parts[PV_MAX_PARTS];
    size_t n, len;
    if (!rel) return pv_strdup("");
    len = strlen(rel);
    buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    n = pv_split(rel, buf, parts, PV_MAX_PARTS);
    name = pv_strdup(n ? parts[n - 1] : "");
    free(buf);
    return name;
}

/* ---- string set (owned) --------------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} pv_set;

static int pv_set_contains(const pv_set *s, const char *v) {
    size_t i;
    if (!v) return 0;
    for (i = 0; i < s->count; i++)
        if (strcmp(s->items[i], v) == 0) return 1;
    return 0;
}

static int pv_set_add(pv_set *s, const char *v) {
    char *dup;
    if (!v) return 1;
    if (pv_set_contains(s, v)) return 1;
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **grown = (char **)realloc(s->items, cap * sizeof(char *));
        if (!grown) return 0;
        s->items = grown;
        s->cap = cap;
    }
    dup = pv_strdup(v);
    if (!dup) return 0;
    s->items[s->count++] = dup;
    return 1;
}

static void pv_set_free(pv_set *s) {
    size_t i;
    for (i = 0; i < s->count; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL;
    s->count = s->cap = 0;
}

/* Add every string element of a JSON array (a non-string is not a module name;
 * the fixtures stay inside that shape). */
static void pv_set_add_json(pv_set *s, const json_t *arr) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING) pv_set_add(s, v->str_val ? v->str_val : "");
    }
}

/* ---- growable string buffer ---------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int is_null;   /* the Python call returned None rather than a string */
} pv_sb;

static int pv_sb_reserve(pv_sb *sb, size_t extra) {
    size_t need = sb->len + extra + 1;
    size_t cap;
    char *grown;
    if (need <= sb->cap) return 1;
    cap = sb->cap ? sb->cap : 32;
    while (cap < need) cap *= 2;
    grown = (char *)realloc(sb->buf, cap);
    if (!grown) return 0;
    sb->buf = grown;
    sb->cap = cap;
    if (!sb->len) sb->buf[0] = '\0';
    return 1;
}

static void pv_sb_puts(pv_sb *sb, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    if (!pv_sb_reserve(sb, n)) return;
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static void pv_sb_putc(pv_sb *sb, char c) {
    if (!pv_sb_reserve(sb, 1)) return;
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

static char *pv_sb_take(pv_sb *sb) {
    char *out = sb->buf;
    if (!out) out = pv_strdup("");
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return out;
}

/* ---- JSON node accessors -------------------------------------------------- */

static const json_t *pv_field(const json_t *o, const char *key) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, key);
}

/* The chain / node spec discriminator; "" when the value carries none. */
static const char *pv_kind(const json_t *n) {
    const json_t *k = pv_field(n, "kind");
    if (!k || k->type != JSON_STRING || !k->str_val) return "";
    return k->str_val;
}

static int pv_is_kind(const json_t *n, const char *kind) {
    return strcmp(pv_kind(n), kind) == 0;
}

/* Whether a dotted module expression's first segment is one of ``roots``. */
static int pv_roots_have(const json_t *roots, const char *s) {
    size_t seglen = 0, i, n;
    if (!s) return 0;
    while (s[seglen] && s[seglen] != '.') seglen++;
    if (!roots || roots->type != JSON_ARRAY) return 0;
    n = json_len(roots);
    for (i = 0; i < n; i++) {
        const json_t *r = json_get(roots, i);
        const char *rs;
        if (!r || r->type != JSON_STRING) continue;
        rs = r->str_val ? r->str_val : "";
        if (strlen(rs) == seglen && memcmp(rs, s, seglen) == 0) return 1;
    }
    return 0;
}

static int pv_json_has_str(const json_t *arr, const char *v) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY || !v) return 0;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && strcmp(e->str_val ? e->str_val : "", v) == 0) return 1;
    }
    return 0;
}

/* ======================================================================== */
/* gate w32_pluginvali_b2daaa_p1                                            */
/* ======================================================================== */

int w32_pluginvali_b2daaa_p1_is_test_file(const char *rel) {
    char *name;
    int out;
    name = pv_basename(rel);
    if (!name) return 0;
    out = pv_startswith(name, "test_") || pv_endswith(name, "_test.py") ||
          strcmp(name, "conftest.py") == 0;
    free(name);
    return out;
}

int w32_pluginvali_b2daaa_p1_should_skip(const char *rel) {
    char *buf;
    const char *parts[PV_MAX_PARTS];
    size_t n, i, len;
    int out = 0;
    if (!rel) return 0;
    len = strlen(rel);
    buf = (char *)malloc(len + 1);
    if (!buf) return 0;
    n = pv_split(rel, buf, parts, PV_MAX_PARTS);
    for (i = 0; i + 1 < n; i++) {
        if (pv_in_list(parts[i], PV_SKIP_DIRS, PV_SKIP_DIRS_N)) {
            out = 1;
            break;
        }
    }
    free(buf);
    if (out) return 1;
    return w32_pluginvali_b2daaa_p1_is_test_file(rel);
}

int w32_pluginvali_b2daaa_p1_core_module_name(const json_t *name, const json_t *roots) {
    if (!name || name->type != JSON_STRING) return 0;
    return pv_roots_have(roots, name->str_val ? name->str_val : "");
}

json_t *w32_pluginvali_b2daaa_p1_core_roots(const json_t *packages, const json_t *modules,
                                            const json_t *moves, const json_t *local) {
    pv_set names = {0};
    pv_set locals = {0};
    const char **kept = NULL;
    char **sorted = NULL;
    json_t *out = json_array();
    size_t i, n, kept_n = 0;

    pv_set_add_json(&names, packages);
    pv_set_add_json(&names, modules);
    if (moves && moves->type == JSON_ARRAY) {
        n = json_len(moves);
        for (i = 0; i < n; i++) {
            const json_t *m = json_get(moves, i);
            size_t len = 0;
            char *seg;
            if (!m || m->type != JSON_STRING || !m->str_val) continue;
            while (m->str_val[len] && m->str_val[len] != '.') len++;
            seg = (char *)malloc(len + 1);
            if (!seg) continue;
            if (len) memcpy(seg, m->str_val, len);
            seg[len] = '\0';
            pv_set_add(&names, seg);
            free(seg);
        }
    }
    pv_set_add(&names, "hermes_plugins");
    pv_set_add_json(&locals, local);

    kept = (const char **)malloc((names.count + 1) * sizeof(char *));
    if (!kept) {
        pv_set_free(&names);
        pv_set_free(&locals);
        return out;
    }
    for (i = 0; i < names.count; i++) {
        const char *v = names.items[i];
        if (strcmp(v, "setup") == 0 || strcmp(v, "conftest") == 0) continue;
        if (pv_set_contains(&locals, v)) continue;
        kept[kept_n++] = v;
    }
    if (kept_n > 1) {
        sorted = (char **)malloc(kept_n * sizeof(char *));
        if (sorted) {
            for (i = 0; i < kept_n; i++) sorted[i] = (char *)kept[i];
            qsort(sorted, kept_n, sizeof(char *), pv_cmp_str);
        }
    }
    for (i = 0; i < kept_n; i++)
        json_append(out, json_string(sorted ? sorted[i] : kept[i]));
    free(sorted);
    free(kept);
    pv_set_free(&names);
    pv_set_free(&locals);
    return out;
}

int w32_pluginvali_b2daaa_p1_lookup_is_core(const json_t *spec, const json_t *constants,
                                            const json_t *roots) {
    const char *kind;
    if (!spec) return 0;
    kind = pv_kind(spec);
    if (strcmp(kind, "name") == 0) {
        const char *id = json_get_str(spec, "id", "");
        const json_t *v = pv_field(constants, id);
        if (!v || v->type != JSON_STRING) return 0;
        return pv_roots_have(roots, v->str_val ? v->str_val : "");
    }
    if (strcmp(kind, "fstring") == 0) {
        const json_t *head = pv_field(spec, "head");
        const char *h;
        if (!head || head->type != JSON_STRING) return 0;
        h = head->str_val ? head->str_val : "";
        if (!strchr(h, '.')) return 0;
        return pv_roots_have(roots, h);
    }
    if (strcmp(kind, "fstring_empty") == 0) return 0;
    if (strcmp(kind, "literal") == 0) {
        const json_t *v = pv_field(spec, "value");
        if (!v || v->type != JSON_STRING) return 0;
        return pv_roots_have(roots, v->str_val ? v->str_val : "");
    }
    return 0;
}

int w32_pluginvali_b2daaa_p1_is_container(const json_t *spec, const json_t *rebound) {
    const char *kind;
    if (!spec) return 0;
    kind = pv_kind(spec);
    if (strcmp(kind, "dict") == 0 || strcmp(kind, "list") == 0 || strcmp(kind, "set") == 0 ||
        strcmp(kind, "dictcomp") == 0 || strcmp(kind, "listcomp") == 0 ||
        strcmp(kind, "setcomp") == 0)
        return 1;
    if (strcmp(kind, "call") == 0) {
        const json_t *func = pv_field(spec, "func");
        const char *id;
        if (!func || !pv_is_kind(func, "name")) return 0;
        id = json_get_str(func, "id", "");
        if (!pv_in_list(id, PV_CONTAINER_CTORS, PV_CONTAINER_CTORS_N)) return 0;
        return !pv_json_has_str(rebound, id);
    }
    return 0;
}

static void pv_collect_bound(const json_t *t, pv_set *out, int depth) {
    const char *kind;
    if (!t || depth > PV_MAX_DEPTH) return;
    kind = pv_kind(t);
    if (strcmp(kind, "name") == 0) {
        const char *id = json_get_str(t, "id", NULL);
        if (id) pv_set_add(out, id);
        return;
    }
    if (strcmp(kind, "tuple") == 0 || strcmp(kind, "list") == 0) {
        const json_t *elts = pv_field(t, "elts");
        size_t i, n;
        if (!elts || elts->type != JSON_ARRAY) return;
        n = json_len(elts);
        for (i = 0; i < n; i++) pv_collect_bound(json_get(elts, i), out, depth + 1);
    }
}

json_t *w32_pluginvali_b2daaa_p1_bound_names(const json_t *targets) {
    pv_set names = {0};
    json_t *out = json_array();
    size_t i, n;
    if (targets && targets->type == JSON_ARRAY) {
        n = json_len(targets);
        for (i = 0; i < n; i++) pv_collect_bound(json_get(targets, i), &names, 0);
    }
    if (names.count > 1) qsort(names.items, names.count, sizeof(char *), pv_cmp_str);
    for (i = 0; i < names.count; i++) json_append(out, json_string(names.items[i]));
    pv_set_free(&names);
    return out;
}

static json_t *pv_sorted_array(const char *const *items, size_t n) {
    const char **tmp;
    json_t *out = json_array();
    size_t i;
    if (!n) return out;
    tmp = (const char **)malloc(n * sizeof(char *));
    if (!tmp) return out;
    for (i = 0; i < n; i++) tmp[i] = items[i];
    qsort(tmp, n, sizeof(char *), pv_cmp_str);
    for (i = 0; i < n; i++) json_append(out, json_string(tmp[i]));
    free(tmp);
    return out;
}

json_t *w32_pluginvali_b2daaa_p1_module_constants(void) {
    json_t *o = json_object();
    json_set(o, "skip_dirs", pv_sorted_array(PV_SKIP_DIRS, PV_SKIP_DIRS_N));
    json_set(o, "container_ctors", pv_sorted_array(PV_CONTAINER_CTORS, PV_CONTAINER_CTORS_N));
    json_set(o, "namespace_escapes", pv_sorted_array(PV_NAMESPACE_ESCAPES, PV_NAMESPACE_ESCAPES_N));
    json_set(o, "mutators", pv_sorted_array(PV_MUTATORS, PV_MUTATORS_N));
    return o;
}

/* ======================================================================== */
/* gate w32_pluginvali_b2daaa_p2                                            */
/* ======================================================================== */

/* Python str() of a segment name: a JSON null renders as "None", not "".
 * _render builds f-strings, so a null attr/id inside a chain lands in the
 * output as the literal text None -- only a TOP-LEVEL Name can return None. */
static void pv_puts_pystr(pv_sb *sb, const json_t *v, const char *key, const json_t *node) {
    const json_t *f = pv_field(node, key);
    (void)v;
    if (!f || f->type == JSON_NULL) {
        pv_sb_puts(sb, "None");
        return;
    }
    if (f->type == JSON_STRING) {
        pv_sb_puts(sb, f->str_val ? f->str_val : "");
        return;
    }
    if (f->type == JSON_BOOL) {
        pv_sb_puts(sb, f->bool_val ? "True" : "False");
        return;
    }
    if (f->type == JSON_NUMBER) {
        char numbuf[64];
        snprintf(numbuf, sizeof numbuf, "%g", f->num_val);
        pv_sb_puts(sb, numbuf);
        return;
    }
    pv_sb_puts(sb, "None");
}

static void pv_render_into(const json_t *n, pv_sb *sb, int depth) {
    const char *kind;
    if (depth > PV_MAX_DEPTH) {
        pv_sb_puts(sb, "<core>");
        return;
    }
    kind = pv_kind(n);
    if (strcmp(kind, "attr") == 0) {
        pv_render_into(pv_field(n, "value"), sb, depth + 1);
        pv_sb_putc(sb, '.');
        pv_puts_pystr(sb, NULL, "attr", n);
    } else if (strcmp(kind, "sub") == 0) {
        pv_render_into(pv_field(n, "value"), sb, depth + 1);
        pv_sb_puts(sb, "[...]");
    } else if (strcmp(kind, "call") == 0) {
        pv_render_into(pv_field(n, "func"), sb, depth + 1);
        pv_sb_puts(sb, "()");
    } else if (strcmp(kind, "name") == 0) {
        const json_t *id = pv_field(n, "id");
        if ((!id || id->type == JSON_NULL) && depth == 0) {
            /* _render(node) for a Name with id None RETURNS None. */
            sb->is_null = 1;
            return;
        }
        pv_puts_pystr(sb, NULL, "id", n);
    } else {
        pv_sb_puts(sb, "<core>");
    }
}

char *w32_pluginvali_b2daaa_p2_render(const json_t *chain) {
    pv_sb sb = {0};
    pv_render_into(chain, &sb, 0);
    if (sb.is_null) {
        free(sb.buf);
        sb.buf = NULL;
        sb.len = sb.cap = 0;
        return NULL;   /* Python returned None */
    }
    return pv_sb_take(&sb);
}

/* Attribute segments (outermost first) plus a trailing bare name; any other
 * terminal keeps the attributes it already collected and contributes no name. */
static void pv_dotted_into(const json_t *chain, pv_sb *sb) {
    const char *segs[PV_MAX_PARTS];
    size_t n = 0, i;
    const json_t *cur = chain;
    const char *name = NULL;
    int first = 1;

    while (n < PV_MAX_PARTS && pv_is_kind(cur, "attr")) {
        segs[n++] = json_get_str(cur, "attr", "");
        cur = pv_field(cur, "value");
    }
    if (pv_is_kind(cur, "name")) name = json_get_str(cur, "id", NULL);
    if (name) {
        pv_sb_puts(sb, name);
        first = 0;
    }
    for (i = n; i > 0; i--) {
        if (!first) pv_sb_putc(sb, '.');
        pv_sb_puts(sb, segs[i - 1]);
        first = 0;
    }
}

char *w32_pluginvali_b2daaa_p2_dotted(const json_t *chain) {
    pv_sb sb = {0};
    pv_dotted_into(chain, &sb);
    return pv_sb_take(&sb);
}

int w32_pluginvali_b2daaa_p2_has_private_segment(const json_t *chain) {
    const json_t *cur = chain;
    int depth = 0;
    while (cur && depth++ <= PV_MAX_DEPTH) {
        const char *kind = pv_kind(cur);
        if (strcmp(kind, "attr") == 0) {
            const char *attr = json_get_str(cur, "attr", "");
            if (attr[0] == '_') return 1;
            cur = pv_field(cur, "value");
        } else if (strcmp(kind, "sub") == 0) {
            cur = pv_field(cur, "value");
        } else if (strcmp(kind, "call") == 0) {
            cur = pv_field(cur, "func");
        } else {
            return 0;
        }
    }
    return 0;
}

int w32_pluginvali_b2daaa_p2_has_mutator_call(const json_t *chain) {
    const json_t *cur = chain;
    int depth = 0;
    while (cur && depth++ <= PV_MAX_DEPTH) {
        const char *kind = pv_kind(cur);
        if (strcmp(kind, "call") == 0) {
            const json_t *func = pv_field(cur, "func");
            if (func && pv_is_kind(func, "attr")) {
                const char *attr = json_get_str(func, "attr", "");
                if (pv_in_list(attr, PV_MUTATORS, PV_MUTATORS_N)) return 1;
            }
            cur = func;
        } else if (strcmp(kind, "attr") == 0 || strcmp(kind, "sub") == 0) {
            cur = pv_field(cur, "value");
        } else {
            return 0;
        }
    }
    return 0;
}

char *w32_pluginvali_b2daaa_p2_func_key(const char *dotted, const json_t *aliases) {
    const char *base;
    const char *last;
    if (!dotted) dotted = "";
    last = strrchr(dotted, '.');
    base = last ? last + 1 : dotted;
    if (aliases && aliases->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(aliases, base);
        if (v && v->type == JSON_STRING) return pv_strdup(v->str_val ? v->str_val : "");
    }
    return pv_strdup(base);
}

static int pv_cmp_int(const void *a, const void *b) {
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

json_t *w32_pluginvali_b2daaa_p2_helper_indices(const json_t *spec, const json_t *table,
                                                const json_t *aliases, const json_t *containers,
                                                const json_t *rebound) {
    json_t *out = json_array();
    const json_t *func, *recv, *indexes;
    char *dotted = NULL, *key = NULL;
    long *vals = NULL;
    size_t n = 0, i;

    if (!spec) return out;
    func = pv_field(spec, "func");
    if (func && pv_is_kind(func, "attr")) {
        recv = pv_field(func, "value");
        if (w32_pluginvali_b2daaa_p1_is_container(recv, rebound)) return out;
        if (recv && pv_is_kind(recv, "name")) {
            const char *id = json_get_str(recv, "id", "");
            if (pv_json_has_str(containers, id)) return out;
        }
    }
    {
        pv_sb sb = {0};
        pv_dotted_into(func, &sb);
        dotted = pv_sb_take(&sb);
    }
    key = w32_pluginvali_b2daaa_p2_func_key(dotted, aliases);
    free(dotted);
    if (!key) return out;

    indexes = pv_field(table, key);
    free(key);
    if (!indexes || indexes->type != JSON_ARRAY) return out;
    if (!json_len(indexes)) return out;
    vals = (long *)malloc(json_len(indexes) * sizeof(long));
    if (!vals) return out;
    for (i = 0; i < json_len(indexes); i++) {
        const json_t *v = json_get(indexes, i);
        if (!v || v->type != JSON_NUMBER) continue;
        vals[n++] = (long)v->num_val;
    }
    if (n > 1) qsort(vals, n, sizeof(long), pv_cmp_int);
    for (i = 0; i < n; i++)
        if (i == 0 || vals[i] != vals[i - 1]) json_append(out, json_number((double)vals[i]));
    free(vals);
    return out;
}

json_t *w32_pluginvali_b2daaa_p2_report(const json_t *hits) {
    static const char *const ADVICE =
        "rebinds Hermes core at runtime (use a public hook, middleware or provider profile): ";
    json_t *o = json_object();
    size_t n = (hits && hits->type == JSON_ARRAY) ? json_len(hits) : 0;
    char *detail;

    if (!n) {
        detail = pv_strdup("no runtime rebinds of Hermes core");
    } else {
        pv_sb sb = {0};
        size_t i, shown = n > 8 ? 8 : n;
        pv_sb_puts(&sb, ADVICE);
        for (i = 0; i < shown; i++) {
            const json_t *h = json_get(hits, i);
            if (i) pv_sb_puts(&sb, "; ");
            pv_sb_puts(&sb, (h && h->type == JSON_STRING && h->str_val) ? h->str_val : "");
        }
        if (n > 8) {
            char tail[48];
            snprintf(tail, sizeof(tail), " (+%lu more)", (unsigned long)(n - 8));
            pv_sb_puts(&sb, tail);
        }
        detail = pv_sb_take(&sb);
    }
    json_set(o, "label", json_string("no core override"));
    json_set(o, "ok", json_bool(n == 0));
    json_set(o, "detail", json_string(detail ? detail : ""));
    free(detail);
    return o;
}
