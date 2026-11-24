/*
 * methods_tools_plugin_servers.c -- real C11 port of the portable-plugin
 * server rows / tool ownership logic of
 * tui_gateway/methods_tools_plugin_servers.py.
 *
 * PoP: w21_methodstoo_16216e_server_identity @ tui_gateway/methods_tools_plugin_servers.py:_server_identity
 * PoP: w21_methodstoo_16216e_portable_server_tools @ tui_gateway/methods_tools_plugin_servers.py:_portable_server_tools
 * PoP: w21_methodstoo_16216e_plugin_tool_ownership @ tui_gateway/methods_tools_plugin_servers.py:_plugin_tool_ownership
 *
 * Faithful details worth keeping:
 *  - the identity comparison happens AFTER the connect-time ${VAR} expansion
 *    (`_interpolate_env_vars`): a same-name native entry that runs a different
 *    command is the user's own server and credits no package;
 *  - `args = cfg.get("args") or ()` then `[args]` for a non-list, so a scalar
 *    or dict argument becomes a one-element tuple of its str();
 *  - names are counted across owners and a name two owners list is dropped
 *    from both (`seen[n] == 1`), which is why the output is sorted per owner.
 */
#include "noc_w21_methodstoo_16216e.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- small helpers -------------------------------------------------------- */

static char *ms_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *ms_dup(const char *s) {
    if (!s) s = "";
    return ms_dup_n(s, strlen(s));
}

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} ms_sb;

static int ms_sb_grow(ms_sb *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *np;
    if (want <= b->cap) return 1;
    if (want < 32) want = 32;
    np = (char *)realloc(b->p, want);
    if (!np) return 0;
    b->p = np;
    b->cap = want;
    return 1;
}

static int ms_sb_raw(ms_sb *b, const char *s, size_t len) {
    if (!ms_sb_grow(b, len)) return 0;
    if (len) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
    return 1;
}

static int ms_sb_puts(ms_sb *b, const char *s) {
    return ms_sb_raw(b, s ? s : "", s ? strlen(s) : 0);
}

static int ms_sb_putc(ms_sb *b, char c) {
    return ms_sb_raw(b, &c, 1);
}

static char *ms_sb_take(ms_sb *b) {
    char *p;
    if (!ms_sb_grow(b, 0)) {
        free(b->p);
        b->p = NULL;
        b->n = b->cap = 0;
        return NULL;
    }
    /* An untouched buffer (empty result, no ms_sb_raw call) still has to be a
     * NUL-terminated string: ms_sb_grow only allocates, it never terminates. */
    p = b->p;
    p[b->n] = '\0';
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static int ms_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- Python str() --------------------------------------------------------- */

static int ms_py_int(const json_t *v, long long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    *out = (long long)d;
    return 1;
}

static void ms_fmt_float(double d, char *buf, size_t n) {
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        snprintf(buf, n, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
}

static int ms_repr_str(ms_sb *b, const char *s) {
    const char *p;
    if (!ms_sb_putc(b, '\'')) return 0;
    for (p = s; *p; p++) {
        if (*p == '\\' || *p == '\'') {
            if (!ms_sb_putc(b, '\\') || !ms_sb_putc(b, *p)) return 0;
        } else if (*p == '\n') {
            if (!ms_sb_puts(b, "\\n")) return 0;
        } else if (*p == '\t') {
            if (!ms_sb_puts(b, "\\t")) return 0;
        } else if (*p == '\r') {
            if (!ms_sb_puts(b, "\\r")) return 0;
        } else if (!ms_sb_putc(b, *p)) {
            return 0;
        }
    }
    return ms_sb_putc(b, '\'');
}

static int ms_repr(ms_sb *b, const json_t *v);

static int ms_emit_str(ms_sb *b, const json_t *v) {
    char num[48];
    if (!v) return ms_sb_puts(b, "None");
    switch (v->type) {
    case JSON_NULL:
        return ms_sb_puts(b, "None");
    case JSON_BOOL:
        return ms_sb_puts(b, v->bool_val ? "True" : "False");
    case JSON_STRING:
        return ms_sb_puts(b, v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        long long i;
        if (ms_py_int(v, &i)) snprintf(num, sizeof(num), "%lld", i);
        else ms_fmt_float(v->num_val, num, sizeof(num));
        return ms_sb_puts(b, num);
    }
    case JSON_ARRAY:
    case JSON_OBJECT:
        return ms_repr(b, v);
    }
    return ms_sb_puts(b, "None");
}

static int ms_repr(ms_sb *b, const json_t *v) {
    size_t i;
    if (!v) return ms_sb_puts(b, "None");
    switch (v->type) {
    case JSON_STRING:
        return ms_repr_str(b, v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        if (!ms_sb_putc(b, '[')) return 0;
        for (i = 0; i < v->c.count; i++) {
            if (i && !ms_sb_puts(b, ", ")) return 0;
            if (!ms_repr(b, v->c.items[i])) return 0;
        }
        return ms_sb_putc(b, ']');
    case JSON_OBJECT:
        if (!ms_sb_putc(b, '{')) return 0;
        for (i = 0; i < v->c.count; i++) {
            const char *k = v->c.keys ? v->c.keys[i] : "";
            if (i && !ms_sb_puts(b, ", ")) return 0;
            if (!ms_repr_str(b, k ? k : "")) return 0;
            if (!ms_sb_puts(b, ": ")) return 0;
            if (!ms_repr(b, v->c.items[i])) return 0;
        }
        return ms_sb_putc(b, '}');
    default:
        return ms_emit_str(b, v);
    }
}

/* str(value or ""); NULL when out of memory. */
static char *ms_str_or_empty(const json_t *v) {
    ms_sb b = {NULL, 0, 0};
    char *out;
    if (!ms_truthy(v)) return ms_dup("");
    if (!ms_emit_str(&b, v)) {
        free(b.p);
        return NULL;
    }
    out = ms_sb_take(&b);
    return out ? out : ms_dup("");
}

/* plain str(value) -- the args generator is `str(a)`, so None stays "None". */
static char *ms_str_of(const json_t *v) {
    ms_sb b = {NULL, 0, 0};
    char *out;
    if (!ms_emit_str(&b, v)) {
        free(b.p);
        return NULL;
    }
    out = ms_sb_take(&b);
    return out ? out : ms_dup("");
}

/* --- ${VAR} expansion (the _interpolate_env_vars seam) -------------------- */

static const char *ms_env_lookup(const json_t *env, const char *name) {
    const json_t *v;
    if (!env || env->type != JSON_OBJECT) return NULL;
    v = json_obj_get(env, name);
    if (!v) return NULL;
    if (v->type != JSON_STRING) return NULL;
    if (!v->str_val || !v->str_val[0]) return NULL;   /* empty resolves to nothing */
    return v->str_val;
}

/* ${...} bodies: strip, then a Cursor-style "env:" prefix, then strip again. */
static char *ms_ref_name(const char *raw, size_t len) {
    size_t i = 0, j = len;
    char *name;
    while (i < j && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\n' || raw[i] == '\r')) i++;
    while (j > i && (raw[j - 1] == ' ' || raw[j - 1] == '\t' || raw[j - 1] == '\n' || raw[j - 1] == '\r')) j--;
    if (j - i >= 4 && !memcmp(raw + i, "env:", 4)) {
        i += 4;
        while (i < j && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\n' || raw[i] == '\r')) i++;
    }
    name = ms_dup_n(raw + i, j - i);
    return name;
}

static char *ms_interp(const char *s, const json_t *env) {
    const char *p = s;
    ms_sb b = {NULL, 0, 0};
    char *out;
    if (!s) return ms_dup("");
    while (*p) {
        const char *open = strstr(p, "${");
        const char *close;
        char *name;
        const char *val;
        if (!open) {
            ms_sb_puts(&b, p);
            break;
        }
        ms_sb_raw(&b, p, (size_t)(open - p));
        close = strchr(open + 2, '}');
        if (!close) {                      /* no } -> the pattern cannot match */
            ms_sb_puts(&b, open);
            break;
        }
        name = ms_ref_name(open + 2, (size_t)(close - (open + 2)));
        val = name ? ms_env_lookup(env, name) : NULL;
        if (val) {
            ms_sb_puts(&b, val);
        } else {
            ms_sb_raw(&b, open, (size_t)(close - open) + 1);   /* keep the literal */
        }
        free(name);
        p = close + 1;
    }
    out = ms_sb_take(&b);
    return out ? out : ms_dup("");
}

/* Recursive form: strings interpolate, dicts/lists rebuild, the rest pass through. */
static json_t *ms_interp_value(const json_t *v, const json_t *env) {
    size_t i;
    json_t *out;
    if (!v) return json_null();
    if (v->type == JSON_STRING) {
        char *s = ms_interp(v->str_val, env);
        out = json_string(s ? s : "");
        free(s);
        return out;
    }
    if (v->type == JSON_ARRAY) {
        out = json_array();
        for (i = 0; i < v->c.count; i++) json_append(out, ms_interp_value(v->c.items[i], env));
        return out;
    }
    if (v->type == JSON_OBJECT) {
        out = json_object();
        for (i = 0; i < v->c.count; i++) {
            const char *k = v->c.keys ? v->c.keys[i] : "";
            json_set(out, k ? k : "", ms_interp_value(v->c.items[i], env));
        }
        return out;
    }
    return json_copy(v);
}

json_t *w21_methodstoo_16216e_server_identity(const json_t *cfg, const json_t *env) {
    json_t *expanded, *args, *out = json_array();
    const json_t *command, *url;
    char *s;

    expanded = ms_interp_value(cfg && cfg->type == JSON_OBJECT ? cfg : json_object(), env);
    if (!expanded || expanded->type != JSON_OBJECT) {
        json_free(expanded);
        expanded = json_object();
    }

    command = json_obj_get(expanded, "command");
    s = ms_str_or_empty(command);
    json_append(out, json_string(s ? s : ""));
    free(s);

    args = json_obj_get(expanded, "args");
    if (!ms_truthy(args)) {
        /* `args = cfg.get("args") or ()` then `[args]` for a non-list: the
         * empty tuple becomes the single argument string "()". */
        json_t *list = json_array();
        json_append(list, json_string("()"));
        json_append(out, list);
    } else if (args->type == JSON_ARRAY) {
        json_t *list = json_array();
        size_t i;
        for (i = 0; i < args->c.count; i++) {
            char *item = ms_str_of(args->c.items[i]);
            json_append(list, json_string(item ? item : ""));
            free(item);
        }
        json_append(out, list);
    } else {
        json_t *list = json_array();
        char *item = ms_str_of(args);
        json_append(list, json_string(item ? item : ""));
        free(item);
        json_append(out, list);
    }

    url = json_obj_get(expanded, "url");
    s = ms_str_or_empty(url);
    json_append(out, json_string(s ? s : ""));
    free(s);

    json_free(expanded);
    return out;
}

/* --- string sets and insertion-ordered maps ------------------------------- */

typedef struct {
    char **v;
    size_t n;
    size_t cap;
} ms_set;

static int ms_set_has(const ms_set *s, const char *name) {
    size_t i;
    for (i = 0; i < s->n; i++) {
        if (!strcmp(s->v[i], name)) return 1;
    }
    return 0;
}

static int ms_set_add(ms_set *s, const char *name) {
    char *copy;
    if (!name) return 1;
    if (ms_set_has(s, name)) return 1;
    if (s->n == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, ncap * sizeof(*nv));
        if (!nv) return 0;
        s->v = nv;
        s->cap = ncap;
    }
    copy = ms_dup(name);
    if (!copy) return 0;
    s->v[s->n++] = copy;
    return 1;
}

static void ms_set_add_all(ms_set *s, const json_t *arr) {
    size_t i;
    if (!arr) return;
    if (arr->type == JSON_ARRAY) {
        for (i = 0; i < arr->c.count; i++) {
            const json_t *v = arr->c.items[i];
            if (v && v->type == JSON_STRING) ms_set_add(s, v->str_val);
        }
        return;
    }
    if (arr->type == JSON_STRING && arr->str_val) {
        /* `names.update("tools")` iterates the characters of a string. */
        const char *p = arr->str_val;
        while (*p) {
            char one[2];
            unsigned char c = (unsigned char)*p;
            size_t len = 1;
            if (c >= 0xF0) len = 4;
            else if (c >= 0xE0) len = 3;
            else if (c >= 0xC0) len = 2;
            if (strlen(p) < len) len = 1;
            memcpy(one, p, len);
            one[len] = '\0';
            ms_set_add(s, one);
            p += len;
        }
        return;
    }
    if (arr->type == JSON_OBJECT) {
        /* `names.update(dict)` iterates the keys. */
        for (i = 0; i < arr->c.count; i++) {
            const char *k = arr->c.keys ? arr->c.keys[i] : NULL;
            if (k) ms_set_add(s, k);
        }
    }
}

static int ms_set_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *ms_set_to_array(ms_set *s) {
    json_t *arr = json_array();
    size_t i;
    if (s->n) qsort(s->v, s->n, sizeof(s->v[0]), ms_set_cmp);
    for (i = 0; i < s->n; i++) json_append(arr, json_string(s->v[i]));
    return arr;
}

static void ms_set_free(ms_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

typedef struct {
    char *key;
    ms_set set;
} ms_entry;

typedef struct {
    ms_entry *e;
    size_t n;
    size_t cap;
} ms_omap;

static ms_set *ms_omap_get(ms_omap *m, const char *key) {
    size_t i;
    char *copy;
    if (!key) key = "";
    for (i = 0; i < m->n; i++) {
        if (!strcmp(m->e[i].key, key)) return &m->e[i].set;
    }
    if (m->n == m->cap) {
        size_t ncap = m->cap ? m->cap * 2 : 8;
        ms_entry *ne = (ms_entry *)realloc(m->e, ncap * sizeof(*ne));
        if (!ne) return NULL;
        m->e = ne;
        m->cap = ncap;
    }
    copy = ms_dup(key);
    if (!copy) return NULL;
    m->e[m->n].key = copy;
    m->e[m->n].set.v = NULL;
    m->e[m->n].set.n = 0;
    m->e[m->n].set.cap = 0;
    m->n++;
    return &m->e[m->n - 1].set;
}

static void ms_omap_free(ms_omap *m) {
    size_t i;
    for (i = 0; i < m->n; i++) {
        free(m->e[i].key);
        ms_set_free(&m->e[i].set);
    }
    free(m->e);
    m->e = NULL;
    m->n = m->cap = 0;
}

/* --- _portable_server_tools ---------------------------------------------- */

static int ms_identity_equal(const json_t *a, const json_t *b, const json_t *env) {
    json_t *left = w21_methodstoo_16216e_server_identity(a, env);
    json_t *right = w21_methodstoo_16216e_server_identity(b, env);
    char *ls = json_dumps(left, 0);
    char *rs = json_dumps(right, 0);
    int same = ls && rs && !strcmp(ls, rs);
    free(ls);
    free(rs);
    json_free(left);
    json_free(right);
    return same;
}

json_t *w21_methodstoo_16216e_portable_server_tools(const json_t *state) {
    const json_t *servers, *native, *declared, *registered, *lazy, *env;
    ms_omap listed;
    json_t *out;
    size_t i;

    listed.e = NULL;
    listed.n = listed.cap = 0;
    if (!state || state->type != JSON_OBJECT) state = json_object();
    servers = json_obj_get(state, "portable_servers");
    native = json_obj_get(state, "native");
    declared = json_obj_get(state, "declared");
    registered = json_obj_get(state, "registered");
    lazy = json_obj_get(state, "lazy");
    env = json_obj_get(state, "env");

    if (servers && servers->type == JSON_OBJECT) {
        for (i = 0; i < servers->c.count; i++) {
            const char *server_name = servers->c.keys ? servers->c.keys[i] : "";
            const json_t *owner_v = servers->c.items[i];
            char *owner_s;
            ms_set *names;
            if (!server_name || !owner_v || owner_v->type == JSON_NULL) continue;
            /* `listed.setdefault(owner, set())` keys by whatever the mapping
             * holds; the only non-string that survives json.dumps is a scalar. */
            owner_s = ms_str_of(owner_v);
            if (!owner_s) continue;
            names = ms_omap_get(&listed, owner_s);
            free(owner_s);
            if (!names) continue;

            if (native && native->type == JSON_OBJECT && json_obj_get(native, server_name)) {
                const json_t *native_cfg = json_obj_get(native, server_name);
                const json_t *declared_cfg = (declared && declared->type == JSON_OBJECT)
                                             ? json_obj_get(declared, server_name) : NULL;
                if (!ms_identity_equal(native_cfg, declared_cfg, env)) continue;
            }
            if (registered && registered->type == JSON_OBJECT) {
                ms_set_add_all(names, json_obj_get(registered, server_name));
            }
            if (lazy && lazy->type == JSON_OBJECT) {
                ms_set_add_all(names, json_obj_get(lazy, server_name));
            }
        }
    }

    out = json_object();
    for (i = 0; i < listed.n; i++) {
        json_set(out, listed.e[i].key, ms_set_to_array(&listed.e[i].set));
    }
    ms_omap_free(&listed);
    return out;
}

/* --- _plugin_tool_ownership ---------------------------------------------- */

json_t *w21_methodstoo_16216e_plugin_tool_ownership(const json_t *state) {
    const json_t *plugins, *core_tools, *toolsets;
    ms_set builtin;
    ms_omap listed;
    json_t *out, *portable;
    size_t i, j;

    builtin.v = NULL;
    builtin.n = builtin.cap = 0;
    listed.e = NULL;
    listed.n = listed.cap = 0;
    if (!state || state->type != JSON_OBJECT) state = json_object();
    plugins = json_obj_get(state, "plugins");
    core_tools = json_obj_get(state, "core_tools");
    toolsets = json_obj_get(state, "toolsets");

    /* builtin = set(_HERMES_CORE_TOOLS) | every TOOLSETS[...]["tools"] */
    ms_set_add_all(&builtin, core_tools);
    if (toolsets && toolsets->type == JSON_OBJECT) {
        for (i = 0; i < toolsets->c.count; i++) {
            const json_t *entry = toolsets->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            ms_set_add_all(&builtin, json_obj_get(entry, "tools"));
        }
    }

    /* listed = {key: set(tools_registered) - builtin} in plugin order */
    if (plugins && plugins->type == JSON_OBJECT) {
        for (i = 0; i < plugins->c.count; i++) {
            const char *key = plugins->c.keys ? plugins->c.keys[i] : "";
            const json_t *tools = plugins->c.items[i];
            ms_set *names = ms_omap_get(&listed, key);
            if (!names) continue;
            if (tools && tools->type == JSON_ARRAY) {
                for (j = 0; j < tools->c.count; j++) {
                    const json_t *t = tools->c.items[j];
                    if (!t || t->type != JSON_STRING || !t->str_val) continue;
                    if (ms_set_has(&builtin, t->str_val)) continue;
                    ms_set_add(names, t->str_val);
                }
            }
        }
    }

    /* then the portable servers' registered/lazy tools per owner */
    portable = w21_methodstoo_16216e_portable_server_tools(state);
    if (portable && portable->type == JSON_OBJECT) {
        for (i = 0; i < portable->c.count; i++) {
            const char *owner = portable->c.keys ? portable->c.keys[i] : "";
            const json_t *names = portable->c.items[i];
            ms_set *target = ms_omap_get(&listed, owner);
            if (!target) continue;
            ms_set_add_all(target, names);
        }
    }
    json_free(portable);

    /* seen counts across owners; a name two owners list belongs to neither */
    out = json_object();
    {
        typedef struct { const char *name; int count; } ms_tally;
        ms_tally *tally = NULL;
        size_t ntally = 0, capl = 0;

        for (i = 0; i < listed.n; i++) {
            for (j = 0; j < listed.e[i].set.n; j++) {
                const char *name = listed.e[i].set.v[j];
                size_t k;
                int found = 0;
                for (k = 0; k < ntally; k++) {
                    if (!strcmp(tally[k].name, name)) {
                        tally[k].count++;
                        found = 1;
                        break;
                    }
                }
                if (found) continue;
                if (ntally == capl) {
                    size_t ncap = capl ? capl * 2 : 8;
                    ms_tally *nt = (ms_tally *)realloc(tally, ncap * sizeof(*nt));
                    if (!nt) break;
                    tally = nt;
                    capl = ncap;
                }
                tally[ntally].name = name;
                tally[ntally].count = 1;
                ntally++;
            }
        }

        for (i = 0; i < listed.n; i++) {
            ms_set keep;
            keep.v = NULL;
            keep.n = keep.cap = 0;
            for (j = 0; j < listed.e[i].set.n; j++) {
                const char *name = listed.e[i].set.v[j];
                size_t k;
                int count = 0;
                for (k = 0; k < ntally; k++) {
                    if (!strcmp(tally[k].name, name)) { count = tally[k].count; break; }
                }
                if (count == 1) ms_set_add(&keep, name);
            }
            json_set(out, listed.e[i].key, ms_set_to_array(&keep));
            ms_set_free(&keep);
        }
        free(tally);
    }

    ms_omap_free(&listed);
    ms_set_free(&builtin);
    return out;
}

