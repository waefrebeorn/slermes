/*
 * left_core_migration_flow.c — hermes_cli/left_core_migration.py FLOW cores +
 * leaves (C11 port). LIVE authority:
 *   C:/Users/eman5/hermes-agent/hermes_cli/left_core_migration.py
 *
 * Self-contained, libjson-only. Ports the DECISION sequences of the left-core
 * feature-table hint builders, the read-only "is this home using the feature"
 * predicates, the notice/scope leaves and the post-split say flow that the
 * sibling ports do NOT claim. Every live effect (config/auth/env reads, path
 * probes, the hermes_constants home/key helpers, the providers registry, the
 * memory_provider_migration collaborators, the injected feature.in_use /
 * feature.notice / say callables, logger.warning and the CLAIMED siblings)
 * enters through ONE universal seam; the cores perform NO I/O:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam records a copy and must not free it); the result is a NEW
 * json_t* the caller frees. NULL == Python None (a scripted JSON null is
 * delivered as a NULL pointer, never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's
 * recording stubs emit the identical shape driving LIVE Python with the same
 * scripts. BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit
 * / CancelledError) PASS THROUGH every `except Exception` site; only plain
 * Exception specs are swallowed there. Fail-closed: with no seam installed the
 * dispatcher returns a RuntimeError raise spec; a missing/empty seam script
 * yields a RuntimeError raise spec (recorded, then propagated/swallowed as live).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. The
 * flows are stateless; _mark mutates its borrowed config dict in place and
 * returns NULL (the harness echoes the mutated config).
 *
 * CLAIMED siblings (ported elsewhere) are SEAMED here, never re-ported:
 *   _toolset_listed, _at_host        (src/hermes_cli/port_left_core_migration.c)
 *   _marked                          (src/hermes_cli/config_migration_flow.c)
 * UNPORTED collaborators are seamed too: _merged_platform_block,
 * routes_at_hosts, _queue_notice, and the memory_provider_migration helpers
 * _install_command / _failed_recently / _note_failure. Internal composition
 * between PORTED functions runs REAL on both sides (no seam event).
 *
 * feature.in_use / feature.notice are injected callables on the LeftCoreFeature
 * row -> always seamed (even when the default is a ported leaf such as
 * _no_notice); the seam args carry the row's plugin name. A feature argument
 * reaches the port as its plugin name; an unknown name models live's
 * `next(f for f in LEFT_CORE if f.plugin == ...)` StopIteration (msg "").
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   get_hermes_home        hermes_constants.get_hermes_home()      args{} ret str
 *   install_command        _install_command(plugin, home)          args{plugin,home} ret str
 *   plugin_load_error      providers.plugin_load_error(plugin)     args{plugin} ret str
 *   get_provider_profile   providers.get_provider_profile(name)    args{name} ret profile|null
 *   read_config_doc        config_backend.read_config_doc(path)    args{path} ret doc|raise
 *   read_json_or_empty     utils.read_json_or_empty(path)          args{path} ret dict
 *   path_is_dir            (home/"plugins"/plugin).is_dir()        args{path} ret bool
 *   path_is_file           (home/"import-sync.json").is_file()     args{path} ret bool
 *   read_yaml_layers       config_loader.read_yaml_layers(home)    args{home} ret dict|raise
 *   resolve_home_dotenv    env_loader.resolve_home_dotenv(home)    args{home} ret dict
 *   current_secret_scope_home secret_scope.current_secret_scope_home() args{} ret str|null
 *   current_secret_scope   secret_scope.current_secret_scope()     args{} ret dict|null
 *   hermes_home_key        hermes_constants.hermes_home_key(home)  args{home} ret str
 *   get_routing_process_hermes_home hermes_constants.get_routing_process_hermes_home() args{} ret str
 *   os_environ_get         os.environ.get(name, "")                args{name} ret str
 *   load_env_file          secret_scope.load_env_file(path)        args{path} ret dict
 *   get_secret             secret_scope.get_secret(name, default)  args{name,default} ret str|null
 *   is_truthy_value        utils.is_truthy_value(value)            args{value} ret bool
 *   toolset_listed         _toolset_listed(config, names, own_platform=) [claimed]
 *                          args{config,names(sorted),own_platform} ret bool
 *   merged_platform_block  _merged_platform_block(home, name) [unported] args{home,name} ret dict|null
 *   platform_config_from_dict PlatformConfig.from_dict({**block,home_channel:None}) args{block} ret desc
 *   in_use                 feature.in_use(home, process_env=)      args{plugin,home,process_env} ret bool
 *   notice                 feature.notice(home, process_env=)      args{plugin,home,process_env} ret str
 *   routes_at_hosts        routes_at_hosts(home, hosts) [unported] args{home,hosts} ret bool
 *   at_host                _at_host(url, host) [claimed]           args{url,host} ret bool
 *   marked                 _marked(config, key, feature) [claimed] args{config,key,plugin} ret bool
 *   queue_notice           _queue_notice(home, message) [unported] args{home,message} ret null
 *   logger_warning         logger.warning(message)                 args{message} ret null
 *   failed_recently        _failed_recently(home, plugin)          args{home,plugin} ret bool
 *   note_failure           _note_failure(home, plugin)             args{home,plugin} ret null
 *   say                    say(message)  (injected sink)           args{message} ret null
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; POSIX lexical paths (home non-empty; backslash->slash,
 *    trailing slashes stripped like pathlib). Paths reach traces as posix str.
 *  - integral JSON numbers read as int; str() of an integral number formats
 *    without a decimal point; non-integral floats use Python repr.
 *  - a PlatformConfig desc is canonicalized to {enabled,token,extra} (defaults
 *    false/null/{}) on BOTH sides; the seam trace records the raw scripted desc.
 *  - frozenset()/set()/tuple returns normalize to sorted/ordered JSON arrays.
 *  - _provider_feature returns the row's plugin name (str) or null.
 *  - logger.debug/info are silent (no seam); logger.warning is seamed.
 *  - the module-level _YUANBAO_OFF_PLATFORMS = _core_platforms_except(...) scan
 *    is outside the fixture domain; off_platforms is read by no ported func.
 *  - str()-reached seam values are scalars (str/number/bool/null); .get/.strip
 *    on a scripted null/non-str models the live AttributeError exactly.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *jstr(const json_t *v) {
    return (is_str(v) && v->str_val) ? v->str_val : NULL;
}
static char *xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* dynamic string buffer */
typedef struct { char *s; size_t len, cap; } lcm_buf_t;
static void lcm_binit(lcm_buf_t *b) { b->s = NULL; b->len = 0; b->cap = 0; }
static void lcm_bputn(lcm_buf_t *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *ns = (char *)realloc(b->s, nc);
        if (!ns) return;
        b->s = ns; b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void lcm_bput(lcm_buf_t *b, const char *s) { if (s) lcm_bputn(b, s, strlen(s)); }
static void lcm_bputc(lcm_buf_t *b, char c) { lcm_bputn(b, &c, 1); }

/* UTF-8 decode (fixture strings are valid UTF-8). */
static unsigned lcm_utf8_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp = s[0];
    if (cp < 0x80) { *p += 1; return cp; }
    int extra = 0;
    if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; extra = 1; }
    else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; extra = 2; }
    else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; extra = 3; }
    else { *p += 1; return cp; }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return s[0]; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += extra + 1;
    return cp;
}

/* Python float repr: shortest round-trip digits, decimal for -4 <= X < 16,
 * scientific otherwise (e+XX, >= 2 exponent digits), always a '.' or 'e'. */
static char *lcm_repr_float(double v) {
    lcm_buf_t b; lcm_binit(&b);
    if (isnan(v)) { lcm_bput(&b, "nan"); return b.s; }
    if (isinf(v)) { lcm_bput(&b, v < 0 ? "-inf" : "inf"); return b.s; }
    if (v == 0.0) { lcm_bput(&b, signbit(v) ? "-0.0" : "0.0"); return b.s; }
    bool neg = signbit(v);
    double a = fabs(v);
    char buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*e", p - 1, a);
        if (strtod(buf, NULL) == a) break;
    }
    if (p > 17) p = 17;
    char digits[32];
    size_t nd = 0;
    const char *q = buf;
    while (*q && *q != 'e' && *q != 'E') {
        if (*q >= '0' && *q <= '9') digits[nd++] = *q;
        q++;
    }
    digits[nd] = '\0';
    int X = (*q) ? atoi(q + 1) : 0;
    if (neg) lcm_bputc(&b, '-');
    if (X >= 16 || X < -4) {
        lcm_bputc(&b, digits[0]);
        if (nd > 1) { lcm_bputc(&b, '.'); lcm_bput(&b, digits + 1); }
        lcm_bputc(&b, 'e');
        lcm_bputc(&b, X < 0 ? '-' : '+');
        char eb[16];
        snprintf(eb, sizeof eb, "%02d", X < 0 ? -X : X);
        lcm_bput(&b, eb);
    } else {
        int id = X + 1;
        if (id <= 0) {
            lcm_bput(&b, "0.");
            for (int i = 0; i < -id; i++) lcm_bputc(&b, '0');
            lcm_bput(&b, digits);
        } else if ((size_t)id >= nd) {
            lcm_bput(&b, digits);
            for (size_t i = nd; i < (size_t)id; i++) lcm_bputc(&b, '0');
            lcm_bput(&b, ".0");
        } else {
            lcm_bputn(&b, digits, (size_t)id);
            lcm_bputc(&b, '.');
            lcm_bput(&b, digits + id);
        }
    }
    return b.s;
}

/* Python str repr: single quotes unless the string has ' and no ";
 * escapes \\, the quote, \n \r \t, C0/C1 controls, NBSP, soft hyphen,
 * U+2028/U+2029; printable non-ASCII stays raw UTF-8. */
static char *lcm_repr_str(const char *s) {
    lcm_buf_t b; lcm_binit(&b);
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    lcm_bputc(&b, quote);
    const char *p = s;
    while (*p) {
        const char *start = p;
        unsigned cp = lcm_utf8_next(&p);
        if (cp == (unsigned)quote || cp == '\\') {
            lcm_bputc(&b, '\\');
            lcm_bputc(&b, (char)cp);
        } else if (cp == '\n') lcm_bput(&b, "\\n");
        else if (cp == '\r') lcm_bput(&b, "\\r");
        else if (cp == '\t') lcm_bput(&b, "\\t");
        else if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f) || cp == 0xa0 ||
                 cp == 0xad || cp == 0x2028 || cp == 0x2029) {
            char eb[16];
            if (cp < 0x100) snprintf(eb, sizeof eb, "\\x%02x", cp);
            else if (cp < 0x10000) snprintf(eb, sizeof eb, "\\u%04x", cp);
            else snprintf(eb, sizeof eb, "\\U%08x", cp);
            lcm_bput(&b, eb);
        } else {
            lcm_bputn(&b, start, (size_t)(p - start));
        }
    }
    lcm_bputc(&b, quote);
    return b.s;
}

static char *lcm_py_repr(const json_t *v) {
    lcm_buf_t b; lcm_binit(&b);
    if (!v || v->type == JSON_NULL) { lcm_bput(&b, "None"); return b.s; }
    switch (v->type) {
        case JSON_BOOL:
            lcm_bput(&b, v->bool_val ? "True" : "False");
            return b.s;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == trunc(d) && fabs(d) < 1e18) {
                char nb[40];
                snprintf(nb, sizeof nb, "%.0f", d);
                lcm_bput(&b, nb);
            } else {
                char *fs = lcm_repr_float(d);
                lcm_bput(&b, fs);
                free(fs);
            }
            return b.s;
        }
        case JSON_STRING: {
            char *rs = lcm_repr_str(v->str_val ? v->str_val : "");
            lcm_bput(&b, rs);
            free(rs);
            return b.s;
        }
        case JSON_ARRAY: {
            lcm_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) lcm_bput(&b, ", ");
                char *rs = lcm_py_repr(v->c.items[i]);
                lcm_bput(&b, rs);
                free(rs);
            }
            lcm_bputc(&b, ']');
            return b.s;
        }
        case JSON_OBJECT: {
            lcm_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) lcm_bput(&b, ", ");
                char *ks = lcm_repr_str(v->c.keys[i] ? v->c.keys[i] : "");
                lcm_bput(&b, ks);
                free(ks);
                lcm_bput(&b, ": ");
                char *rs = lcm_py_repr(v->c.items[i]);
                lcm_bput(&b, rs);
                free(rs);
            }
            lcm_bputc(&b, '}');
            return b.s;
        }
        default:
            lcm_bput(&b, "None");
            return b.s;
    }
}

/* str(v): strings verbatim, everything else repr(v). */
static char *lcm_py_str(const json_t *v) {
    if (is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return lcm_py_repr(v);
}
/* str(v or ""): the falsy branch collapses to the EMPTY STRING (not "None"). */
static char *lcm_str_or_empty(const json_t *v) {
    return jtruth(v) ? lcm_py_str(v) : strdup("");
}
/* str(v or "").strip() — frees the intermediate. */
static char *lcm_strip(const char *s);
static char *lcm_strip_or_empty(const json_t *v) {
    char *raw = lcm_str_or_empty(v);
    char *out = lcm_strip(raw);
    free(raw);
    return out;
}

/* Python type name for AttributeError messages. Integral numbers read as int
 * (libjson has no int/float identity; the fixture domain keeps integral
 * literals integral and non-integral literals non-integral). */
static const char *lcm_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == trunc(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* str.strip() over the ASCII White_Space set (0x09-0x0d, 0x1c-0x1f, 0x20). */
static bool lcm_is_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *lcm_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && lcm_is_ws((unsigned char)s[a])) a++;
    while (b > a && lcm_is_ws((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* ASCII str.lower() (fixture domain is ASCII). */
static char *lcm_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    return out;
}
/* str.replace(old, new): all occurrences. */
static char *lcm_replace(const char *s, const char *old, const char *nw) {
    if (!s) s = "";
    size_t ol = strlen(old);
    if (ol == 0) return strdup(s);
    lcm_buf_t b; lcm_binit(&b);
    const char *p = s;
    while (*p) {
        if (strncmp(p, old, ol) == 0) { lcm_bput(&b, nw); p += ol; }
        else { lcm_bputc(&b, *p); p++; }
    }
    return b.s ? b.s : strdup("");
}

/* ── POSIX lexical path helpers (mirror pathlib normalization) ── */
static char *lcm_path_posix(const char *p) {
    if (!p) return strdup("");
    char *s = strdup(p);
    for (char *q = s; *q; q++) if (*q == '\\') *q = '/';
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') s[--n] = '\0';
    return s;
}
static char *lcm_path_join(const char *a, const char *b) {
    if (!b || !*b) return strdup(a ? a : "");
    if (b[0] == '/') return strdup(b);
    if (!a || !*a) return strdup(b);
    size_t na = strlen(a);
    if (a[na - 1] == '/') return xsf("%s%s", a, b);
    return xsf("%s/%s", a, b);
}

/* ── string vector + sorted set ── */
typedef struct { char **v; size_t n, cap; } lcm_sv_t;
static void lcm_sv_init(lcm_sv_t *sv) { sv->v = NULL; sv->n = 0; sv->cap = 0; }
static void lcm_sv_push(lcm_sv_t *sv, const char *s) {
    if (sv->n == sv->cap) {
        size_t nc = sv->cap ? sv->cap * 2 : 8;
        char **nv = (char **)realloc(sv->v, nc * sizeof(char *));
        if (!nv) return;
        sv->v = nv; sv->cap = nc;
    }
    sv->v[sv->n++] = strdup(s ? s : "");
}
static void lcm_sv_free(lcm_sv_t *sv) {
    for (size_t i = 0; i < sv->n; i++) free(sv->v[i]);
    free(sv->v);
    sv->v = NULL; sv->n = sv->cap = 0;
}
static int lcm_sv_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* sorted({items}) -> JSON array of strings (dedup). Does not free sv. */
static json_t *lcm_sorted_set(lcm_sv_t *sv) {
    if (sv->n > 1) qsort(sv->v, sv->n, sizeof(char *), lcm_sv_cmp);
    json_t *arr = json_array();
    for (size_t i = 0; i < sv->n; i++) {
        if (i > 0 && strcmp(sv->v[i], sv->v[i - 1]) == 0) continue;
        json_append(arr, json_string(sv->v[i]));
    }
    return arr;
}
static bool lcm_str_in(const char *s, const char *const *names, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, names[i]) == 0) return true;
    return false;
}

/* ── universal seam ── */
typedef struct lcm_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} lcm_seams_t;

static json_t *lcm_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool lcm_is_raise(const json_t *r) {
    return is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *lcm_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* BaseException kinds that pass through `except Exception`. */
static bool lcm_is_base_exc(const char *c) {
    return !strcmp(c, "SystemExit") || !strcmp(c, "KeyboardInterrupt")
        || !strcmp(c, "GeneratorExit") || !strcmp(c, "CancelledError");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: lcm_call OWNS args (frees it after the call); the seam BORROWS
 * args (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *lcm_call(const lcm_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return lcm_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *lcm_args0(void) { return json_object(); }
static void lcm_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void lcm_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, jpresent(v) ? json_copy(v) : json_null());
}
/* AttributeError "'<T>' object has no attribute '<attr>'". */
static json_t *lcm_attr_raise(const json_t *v, const char *attr) {
    char *m = xsf("'%s' object has no attribute '%s'", lcm_typename(v), attr);
    json_t *r = lcm_raise("AttributeError", m);
    free(m);
    return r;
}
static json_t *lcm_stop_iteration(void) { return lcm_raise("StopIteration", ""); }

/* ── LEFT_CORE feature table (transcribed from the quarry; only the fields
 *    read by ported functions are carried) ── */
typedef struct {
    const char *plugin;
    const char *label;
    const char *platform;
    const char *cli;
    const char *command;
    const char *old_command;
    const char *const *old_cli; size_t n_old_cli;
    const char *plugin_key;
    const char *const *plugin_names; size_t n_plugin_names;
    const char *terminal_backend;
    const char *const *providers; size_t n_providers;
    const char *const *profile_env; size_t n_profile_env;
    const char *const *launch_env; size_t n_launch_env;
    const char *const *hosts; size_t n_hosts;
    const char *const *toolsets; size_t n_toolsets;
} lcm_feature_t;

static const char *const LCM_COPILOT_PROVIDERS[] = {
    "copilot", "github-copilot", "github-models", "github-model", "github",
    "copilot-acp", "github-copilot-acp", "copilot-acp-agent"};
static const char *const LCM_COPILOT_PROFILE_ENV[] = {
    "HERMES_COPILOT_ACP_COMMAND", "HERMES_COPILOT_ACP_ARGS", "COPILOT_CLI_PATH",
    "COPILOT_ACP_BASE_URL"};
static const char *const LCM_COPILOT_LAUNCH_ENV[] = {
    "HERMES_COPILOT_ACP_COMMAND", "HERMES_COPILOT_ACP_ARGS", "COPILOT_CLI_PATH"};
static const char *const LCM_COPILOT_HOSTS[] = {"githubcopilot.com", "models.github.ai"};
static const char *const LCM_HA_TOOLSETS[] = {"homeassistant"};
static const char *const LCM_CAMOFOX_TOOLSETS[] = {"camofox"};
static const char *const LCM_YUANBAO_TOOLSETS[] = {"yuanbao"};
static const char *const LCM_YUANBAO_PLUGIN_NAMES[] = {"yuanbao-platform"};
static const char *const LCM_IMPORTERS_OLD_CLI[] = {"claw"};

static const lcm_feature_t LCM_LEFT_CORE[] = {
    {"homeassistant", "Home Assistant", "homeassistant", "", "", "",
     NULL, 0, "", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0, NULL, 0,
     LCM_HA_TOOLSETS, 1},
    {"spotify", "Spotify", "", "spotify", "", "",
     NULL, 0, "", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0},
    {"langfuse", "Langfuse", "", "", "", "",
     NULL, 0, "observability/langfuse", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0,
     NULL, 0, NULL, 0},
    {"mcp-serve", "Hermes as an MCP server", "", "", "mcp-serve", "hermes mcp serve",
     NULL, 0, "", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0},
    {"camofox", "Camofox browser backend", "", "", "", "",
     NULL, 0, "", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0, NULL, 0,
     LCM_CAMOFOX_TOOLSETS, 1},
    {"singularity", "Singularity/Apptainer terminal backend", "", "", "", "",
     NULL, 0, "", NULL, 0, "singularity", NULL, 0, NULL, 0, NULL, 0, NULL, 0,
     NULL, 0},
    {"agent-importers", "`hermes import-agent`", "", "", "import-agent", "",
     LCM_IMPORTERS_OLD_CLI, 1, "", NULL, 0, "", NULL, 0, NULL, 0, NULL, 0, NULL, 0,
     NULL, 0},
    {"github-copilot", "GitHub Copilot", "", "", "", "",
     NULL, 0, "", NULL, 0, "",
     LCM_COPILOT_PROVIDERS, 8, LCM_COPILOT_PROFILE_ENV, 4, LCM_COPILOT_LAUNCH_ENV, 3,
     LCM_COPILOT_HOSTS, 2, NULL, 0},
    {"yuanbao", "Yuanbao", "yuanbao", "", "", "",
     NULL, 0, "platforms/yuanbao", LCM_YUANBAO_PLUGIN_NAMES, 1, "", NULL, 0, NULL, 0,
     NULL, 0, NULL, 0, LCM_YUANBAO_TOOLSETS, 1},
};
static const size_t LCM_N_FEATURES = sizeof(LCM_LEFT_CORE) / sizeof(LCM_LEFT_CORE[0]);

static const lcm_feature_t *lcm_feature(const char *plugin) {
    if (!plugin) return NULL;
    for (size_t i = 0; i < LCM_N_FEATURES; i++)
        if (strcmp(LCM_LEFT_CORE[i].plugin, plugin) == 0) return &LCM_LEFT_CORE[i];
    return NULL;
}

#define LCM_YUANBAO_NOTICE \
    "Yuanbao is now a Hermes Labs plugin: built by Nous Research, not supported, " \
    "and likely not maintained going forward. Fixes from the community are welcome."

/* ═══════════════════════ table / hint leaves ═══════════════════════ */

/* PoP: lcm_no_notice @ hermes_cli/left_core_migration.py:_no_notice */
static json_t *lcm_no_notice(const lcm_seams_t *s, const char *home, bool process_env) {
    (void)s; (void)home; (void)process_env;
    return json_string("");
}

/* PoP: lcm_profile_env_names @ hermes_cli/left_core_migration.py:profile_env_names */
static json_t *lcm_profile_env_names(const lcm_seams_t *s) {
    (void)s;
    lcm_sv_t sv; lcm_sv_init(&sv);
    for (size_t i = 0; i < LCM_N_FEATURES; i++)
        for (size_t j = 0; j < LCM_LEFT_CORE[i].n_profile_env; j++)
            lcm_sv_push(&sv, LCM_LEFT_CORE[i].profile_env[j]);
    json_t *r = lcm_sorted_set(&sv);
    lcm_sv_free(&sv);
    return r;
}

/* PoP: lcm_launch_env_names @ hermes_cli/left_core_migration.py:launch_env_names */
static json_t *lcm_launch_env_names(const lcm_seams_t *s) {
    (void)s;
    lcm_sv_t sv; lcm_sv_init(&sv);
    for (size_t i = 0; i < LCM_N_FEATURES; i++)
        for (size_t j = 0; j < LCM_LEFT_CORE[i].n_launch_env; j++)
            lcm_sv_push(&sv, LCM_LEFT_CORE[i].launch_env[j]);
    json_t *r = lcm_sorted_set(&sv);
    lcm_sv_free(&sv);
    return r;
}

/* PoP: lcm_legacy_plugin_alias @ hermes_cli/left_core_migration.py:legacy_plugin_alias */
static json_t *lcm_legacy_plugin_alias(const lcm_seams_t *s, const char *name) {
    (void)s;
    const char *nm = name ? name : "";
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (!f->plugin_key[0]) continue;
        if (strcmp(nm, f->plugin_key) == 0 ||
            lcm_str_in(nm, f->plugin_names, f->n_plugin_names))
            return json_string(f->plugin);
    }
    return json_string("");
}

/* PoP: lcm_legacy_plugin_keys @ hermes_cli/left_core_migration.py:legacy_plugin_keys */
static json_t *lcm_legacy_plugin_keys(const lcm_seams_t *s, const char *plugin) {
    (void)s;
    const char *pl = plugin ? plugin : "";
    lcm_sv_t sv; lcm_sv_init(&sv);
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (!f->plugin_key[0] || strcmp(f->plugin, pl) != 0) continue;
        lcm_sv_push(&sv, f->plugin_key);
        for (size_t j = 0; j < f->n_plugin_names; j++)
            lcm_sv_push(&sv, f->plugin_names[j]);
    }
    json_t *r = lcm_sorted_set(&sv);
    lcm_sv_free(&sv);
    return r;
}

/* PoP: lcm_provider_feature @ hermes_cli/left_core_migration.py:_provider_feature */
/* Returns the matched row's plugin name (str) or NULL (None). */
static json_t *lcm_provider_feature(const lcm_seams_t *s, const json_t *name) {
    (void)s;
    char *raw = jtruth(name) ? lcm_py_str(name) : strdup("");
    char *st = lcm_strip(raw); free(raw);
    char *key = lcm_lower(st); free(st);
    const lcm_feature_t *found = NULL;
    if (key[0]) {
        for (size_t i = 0; i < LCM_N_FEATURES; i++) {
            const lcm_feature_t *f = &LCM_LEFT_CORE[i];
            if (lcm_str_in(key, f->providers, f->n_providers)) { found = f; break; }
        }
    }
    free(key);
    return found ? json_string(found->plugin) : NULL;
}

/* PoP: lcm_install_sentence @ hermes_cli/left_core_migration.py:_install_sentence */
static json_t *lcm_install_sentence(const lcm_seams_t *s, const char *feature_plugin) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    json_t *hr = lcm_call(s, "get_hermes_home", lcm_args0());
    if (lcm_is_raise(hr)) return hr;
    char *home = lcm_path_posix(jstr(hr));
    json_free(hr);
    json_t *a = json_object();
    lcm_set_str(a, "plugin", f->plugin);
    lcm_set_str(a, "home", home);
    json_t *cmd = lcm_call(s, "install_command", a);
    free(home);
    if (lcm_is_raise(cmd)) return cmd;
    char *cs = lcm_py_str(cmd);
    json_free(cmd);
    char *out = xsf("%s moved out of core into the '%s' plugin: install it with `%s`",
                    f->label, f->plugin, cs);
    free(cs);
    return json_string(out);
}

/* PoP: lcm_platform_install_hint @ hermes_cli/left_core_migration.py:platform_install_hint */
static json_t *lcm_platform_install_hint(const lcm_seams_t *s, const char *platform) {
    const char *pl = platform ? platform : "";
    const lcm_feature_t *found = NULL;
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (f->platform[0] && strcmp(f->platform, pl) == 0) { found = f; break; }
    }
    if (!found) return json_string("");
    json_t *sent = lcm_install_sentence(s, found->plugin);
    if (lcm_is_raise(sent)) return sent;
    char *ss = lcm_py_str(sent);
    json_free(sent);
    char *out = xsf(". %s", ss);
    free(ss);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: lcm_terminal_backend_install_hint @ hermes_cli/left_core_migration.py:terminal_backend_install_hint */
static json_t *lcm_terminal_backend_install_hint(const lcm_seams_t *s, const char *backend) {
    const char *bk = backend ? backend : "";
    const lcm_feature_t *found = NULL;
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (f->terminal_backend[0] && strcmp(f->terminal_backend, bk) == 0) { found = f; break; }
    }
    if (!found) return json_string("");
    json_t *sent = lcm_install_sentence(s, found->plugin);
    if (lcm_is_raise(sent)) return sent;
    char *ss = lcm_py_str(sent);
    json_free(sent);
    char *out = xsf(". %s", ss);
    free(ss);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: lcm_moved_plugin_hint @ hermes_cli/left_core_migration.py:moved_plugin_hint */
static json_t *lcm_moved_plugin_hint(const lcm_seams_t *s, const char *name) {
    const char *nm = name ? name : "";
    const lcm_feature_t *found = NULL;
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (f->plugin_key[0] &&
            (strcmp(nm, f->plugin_key) == 0 || strcmp(nm, f->plugin) == 0)) { found = f; break; }
    }
    if (!found) return json_string("");
    json_t *sent = lcm_install_sentence(s, found->plugin);
    if (lcm_is_raise(sent)) return sent;
    char *ss = lcm_py_str(sent);
    json_free(sent);
    char *out = xsf("%s.", ss);
    free(ss);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: lcm_load_error @ hermes_cli/left_core_migration.py:_load_error */
static json_t *lcm_load_error(const lcm_seams_t *s, const char *feature_plugin) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    json_t *a = json_object();
    lcm_set_str(a, "plugin", f->plugin);
    return lcm_call(s, "plugin_load_error", a);
}

/* PoP: lcm_provider_left_core @ hermes_cli/left_core_migration.py:provider_left_core */
static json_t *lcm_provider_left_core(const lcm_seams_t *s, const json_t *name) {
    json_t *feat = lcm_provider_feature(s, name);
    if (!feat) return json_bool(false);
    json_free(feat);
    char *raw = lcm_py_str(name);
    char *st = lcm_strip(raw); free(raw);
    char *key = lcm_lower(st); free(st);
    json_t *a = json_object();
    lcm_set_str(a, "name", key);
    free(key);
    json_t *prof = lcm_call(s, "get_provider_profile", a);
    if (lcm_is_raise(prof)) return prof;
    bool none = (prof == NULL);
    json_free(prof);
    return json_bool(none);
}

/* forward decls for real composition */
static json_t *lcm_plugin_present(const lcm_seams_t *s, const char *plugin, const char *home);
static json_t *lcm_plugin_lists(const lcm_seams_t *s, const char *home, const char *plugin,
                                const char *const *also, size_t n_also);
static json_t *lcm_noticed(const lcm_seams_t *s, const char *message, const char *home,
                           const char *feature_plugin, bool process_env);

/* PoP: lcm_provider_install_hint @ hermes_cli/left_core_migration.py:provider_install_hint */
static json_t *lcm_provider_install_hint(const lcm_seams_t *s, const json_t *name) {
    json_t *feat = lcm_provider_feature(s, name);
    if (!feat) return json_string("");
    const lcm_feature_t *f = lcm_feature(jstr(feat));
    json_free(feat);
    if (!f) return lcm_stop_iteration();
    json_t *hr = lcm_call(s, "get_hermes_home", lcm_args0());
    if (lcm_is_raise(hr)) return hr;
    char *home = lcm_path_posix(jstr(hr));
    json_free(hr);
    json_t *pres = lcm_plugin_present(s, f->plugin, home);
    if (lcm_is_raise(pres)) { free(home); return pres; }
    bool present = jtruth(pres);
    json_free(pres);
    if (!present) {
        free(home);
        json_t *sent = lcm_install_sentence(s, f->plugin);
        if (lcm_is_raise(sent)) return sent;
        char *ss = lcm_py_str(sent);
        json_free(sent);
        char *out = xsf("%s.", ss);
        free(ss);
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    json_t *a = json_object();
    lcm_set_str(a, "plugin", f->plugin);
    lcm_set_str(a, "home", home);
    json_t *cmd = lcm_call(s, "install_command", a);
    if (lcm_is_raise(cmd)) { free(home); return cmd; }
    char *cs = lcm_py_str(cmd);
    json_free(cmd);
    json_t *lists = lcm_plugin_lists(s, home, f->plugin, NULL, 0);
    free(home);
    if (lcm_is_raise(lists)) { free(cs); return lists; }
    bool disabled = jtruth(json_get(lists, 1));
    json_free(lists);
    char *out;
    if (disabled) {
        char *en = lcm_replace(cs, " install ", " enable ");
        out = xsf("%s moved out of core into the '%s' plugin, which is installed but "
                  "disabled: run `%s`.", f->label, f->plugin, en);
        free(en);
    } else {
        json_t *err = lcm_load_error(s, f->plugin);
        if (lcm_is_raise(err)) { free(cs); return err; }
        char *es = lcm_py_str(err);
        json_free(err);
        char *errpart = es[0] ? xsf(" (%s)", es) : strdup("");
        free(es);
        char *up = lcm_replace(cs, " install ", " update ");
        out = xsf("%s moved out of core into the '%s' plugin, which is installed but failed "
                  "to load%s: run `%s`, or see `hermes logs` for the cause.",
                  f->label, f->plugin, errpart, up);
        free(errpart); free(up);
    }
    free(cs);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: lcm_feature_in_use @ hermes_cli/left_core_migration.py:feature_in_use */
static json_t *lcm_feature_in_use(const lcm_seams_t *s, const char *feature_plugin,
                                  const char *home, bool process_env) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    char *hp = lcm_path_posix(home);
    json_t *a = json_object();
    lcm_set_str(a, "plugin", f->plugin);
    lcm_set_str(a, "home", hp);
    json_set(a, "process_env", json_bool(process_env));
    json_t *iu = lcm_call(s, "in_use", a);
    if (lcm_is_raise(iu)) { free(hp); return iu; }
    if (jtruth(iu)) { json_free(iu); free(hp); return json_bool(true); }
    json_free(iu);
    if (f->n_hosts > 0) {
        json_t *hosts = json_array();
        for (size_t i = 0; i < f->n_hosts; i++) json_append(hosts, json_string(f->hosts[i]));
        json_t *b = json_object();
        lcm_set_str(b, "home", hp);
        json_set(b, "hosts", hosts);
        json_t *rh = lcm_call(s, "routes_at_hosts", b);
        free(hp);
        if (lcm_is_raise(rh)) return rh;
        bool t = jtruth(rh);
        json_free(rh);
        return json_bool(t);
    }
    free(hp);
    return json_bool(false);
}

/* PoP: lcm_host_install_hint @ hermes_cli/left_core_migration.py:host_install_hint */
static json_t *lcm_host_install_hint(const lcm_seams_t *s, const json_t *base_url) {
    char *url = jtruth(base_url) ? lcm_py_str(base_url) : strdup("");
    const lcm_feature_t *found = NULL;
    for (size_t i = 0; i < LCM_N_FEATURES && !found; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        for (size_t j = 0; j < f->n_hosts; j++) {
            json_t *a = json_object();
            lcm_set_str(a, "url", url);
            lcm_set_str(a, "host", f->hosts[j]);
            json_t *r = lcm_call(s, "at_host", a);
            if (lcm_is_raise(r)) { free(url); return r; }
            bool m = jtruth(r);
            json_free(r);
            if (m) { found = f; break; }
        }
    }
    free(url);
    if (!found) return json_string("");
    for (size_t i = 0; i < found->n_providers; i++) {
        json_t *nm = json_string(found->providers[i]);
        json_t *pl = lcm_provider_left_core(s, nm);
        json_free(nm);
        if (lcm_is_raise(pl)) return pl;
        bool left = jtruth(pl);
        json_free(pl);
        if (!left) return json_string("");
    }
    json_t *nm0 = json_string(found->providers[0]);
    json_t *hint = lcm_provider_install_hint(s, nm0);
    json_free(nm0);
    return hint;
}

/* PoP: lcm_moved_command_hint @ hermes_cli/left_core_migration.py:moved_command_hint */
static json_t *lcm_moved_command_hint(const lcm_seams_t *s, const char *prog,
                                      const char *value, const char *action) {
    const char *pg = prog ? prog : "";
    const char *vl = value ? value : "";
    const char *ac = action ? action : "";
    json_t *hr = lcm_call(s, "get_hermes_home", lcm_args0());
    if (lcm_is_raise(hr)) return hr;
    char *home = lcm_path_posix(jstr(hr));
    json_free(hr);
    char *joined = xsf("%s %s", pg, vl);
    const lcm_feature_t *moved = NULL;
    for (size_t i = 0; i < LCM_N_FEATURES; i++) {
        const lcm_feature_t *f = &LCM_LEFT_CORE[i];
        if (f->old_command[0] && strcmp(f->old_command, joined) == 0) { moved = f; break; }
    }
    free(joined);
    if (moved) {
        json_t *pres = lcm_plugin_present(s, moved->plugin, home);
        if (lcm_is_raise(pres)) { free(home); return pres; }
        bool present = jtruth(pres);
        json_free(pres);
        char *get = strdup("");
        if (!present) {
            json_t *a = json_object();
            lcm_set_str(a, "plugin", moved->plugin);
            lcm_set_str(a, "home", home);
            json_t *cmd = lcm_call(s, "install_command", a);
            if (lcm_is_raise(cmd)) { free(home); free(get); return cmd; }
            char *cs = lcm_py_str(cmd);
            json_free(cmd);
            free(get);
            get = xsf(" Install it with: `%s`.", cs);
            free(cs);
        }
        free(home);
        char *out = xsf("%s moved out of core into the '%s' plugin: `%s` is now "
                        "`hermes %s`.%s", moved->label, moved->plugin,
                        moved->old_command, moved->command, get);
        free(get);
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    const lcm_feature_t *feature = NULL;
    if (strcmp(pg, "hermes auth") == 0) {
        for (size_t i = 0; i < LCM_N_FEATURES; i++) {
            const lcm_feature_t *f = &LCM_LEFT_CORE[i];
            if (f->cli[0] && strcmp(f->cli, vl) == 0) { feature = f; break; }
        }
    } else if (strcmp(pg, "hermes") == 0) {
        if (vl[0]) {
            for (size_t i = 0; i < LCM_N_FEATURES; i++) {
                const lcm_feature_t *f = &LCM_LEFT_CORE[i];
                if (strcmp(vl, f->cli) == 0 || strcmp(vl, f->command) == 0 ||
                    lcm_str_in(vl, f->old_cli, f->n_old_cli)) { feature = f; break; }
            }
        }
    }
    if (!feature) { free(home); return json_string(""); }
    json_t *a = json_object();
    lcm_set_str(a, "plugin", feature->plugin);
    lcm_set_str(a, "home", home);
    json_t *inst = lcm_call(s, "install_command", a);
    if (lcm_is_raise(inst)) { free(home); return inst; }
    char *is = lcm_py_str(inst);
    json_free(inst);
    json_t *pres = lcm_plugin_present(s, feature->plugin, home);
    if (lcm_is_raise(pres)) { free(home); free(is); return pres; }
    bool present = jtruth(pres);
    json_free(pres);
    free(home);
    char *get;
    if (!present) {
        get = xsf(" Install it with: `%s`.", is);
    } else if (strcmp(pg, "hermes") == 0) {
        char *en = lcm_replace(is, " install ", " enable ");
        get = xsf(" Enable it with: `%s`.", en);
        free(en);
    } else {
        get = strdup("");
    }
    free(is);
    char *out;
    if (strcmp(pg, "hermes auth") == 0) {
        lcm_buf_t ob; lcm_binit(&ob);
        lcm_bput(&ob, "hermes auth");
        if (ac[0]) { lcm_bputc(&ob, ' '); lcm_bput(&ob, ac); }
        if (vl[0]) { lcm_bputc(&ob, ' '); lcm_bput(&ob, vl); }
        const char *act = ac[0] ? ac : "login";
        out = xsf("%s moved out of core into the '%s' plugin: `%s` is now "
                  "`hermes %s %s`.%s", feature->label, feature->plugin,
                  ob.s ? ob.s : "", vl, act, get);
        free(ob.s);
    } else {
        char *label = lcm_str_in(vl, feature->old_cli, feature->n_old_cli)
                          ? xsf("`hermes %s`", vl) : strdup(feature->label);
        out = xsf("%s moved out of core into the '%s' plugin, which is not loaded.%s",
                  label, feature->plugin, get);
        free(label);
    }
    free(get);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: lcm_noticed @ hermes_cli/left_core_migration.py:_noticed */
static json_t *lcm_noticed(const lcm_seams_t *s, const char *message, const char *home,
                           const char *feature_plugin, bool process_env) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    char *hp = lcm_path_posix(home);
    json_t *a = json_object();
    lcm_set_str(a, "plugin", f->plugin);
    lcm_set_str(a, "home", hp);
    json_set(a, "process_env", json_bool(process_env));
    free(hp);
    json_t *extra = lcm_call(s, "notice", a);
    if (lcm_is_raise(extra)) return extra;
    const char *msg = message ? message : "";
    json_t *r;
    if (jtruth(extra)) {
        char *es = lcm_py_str(extra);
        char *out = xsf("%s %s", msg, es);
        free(es);
        r = json_string(out);
        free(out);
    } else {
        r = json_string(msg);
    }
    json_free(extra);
    return r;
}

/* ═══════════════════════ read-only predicates ═══════════════════════ */

/* PoP: lcm_read_config @ hermes_cli/left_core_migration.py:_read_config */
static json_t *lcm_read_config(const lcm_seams_t *s, const char *home) {
    char *hp = lcm_path_posix(home);
    char *path = lcm_path_join(hp, "config.yaml");
    free(hp);
    json_t *a = json_object();
    lcm_set_str(a, "path", path);
    free(path);
    json_t *r = lcm_call(s, "read_config_doc", a);
    if (lcm_is_raise(r)) {
        if (lcm_is_base_exc(lcm_raise_class(r))) return r;
        json_free(r);
        return json_object();
    }
    if (is_obj(r)) return r;
    json_free(r);
    return json_object();
}

/* PoP: lcm_auth_store @ hermes_cli/left_core_migration.py:_auth_store */
static json_t *lcm_auth_store(const lcm_seams_t *s, const char *home) {
    char *hp = lcm_path_posix(home);
    char *path = lcm_path_join(hp, "auth.json");
    free(hp);
    json_t *a = json_object();
    lcm_set_str(a, "path", path);
    free(path);
    return lcm_call(s, "read_json_or_empty", a);
}

/* PoP: lcm_plugin_present @ hermes_cli/left_core_migration.py:plugin_present */
static json_t *lcm_plugin_present(const lcm_seams_t *s, const char *plugin, const char *home) {
    char *hp = lcm_path_posix(home);
    char *p1 = lcm_path_join(hp, "plugins");
    free(hp);
    char *path = lcm_path_join(p1, plugin ? plugin : "");
    free(p1);
    json_t *a = json_object();
    lcm_set_str(a, "path", path);
    free(path);
    return lcm_call(s, "path_is_dir", a);
}

/* PoP: lcm_agent_imports_in_use @ hermes_cli/left_core_migration.py:agent_imports_in_use */
static json_t *lcm_agent_imports_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    (void)process_env;
    char *hp = lcm_path_posix(home);
    char *path = lcm_path_join(hp, "import-sync.json");
    free(hp);
    json_t *a = json_object();
    lcm_set_str(a, "path", path);
    free(path);
    return lcm_call(s, "path_is_file", a);
}

/* PoP: lcm_plugins_listed @ hermes_cli/left_core_migration.py:_plugins_listed */
static json_t *lcm_plugins_listed(const lcm_seams_t *s, const char *home,
                                  const char *const *names, size_t n_names) {
    char *hp = lcm_path_posix(home);
    json_t *a = json_object();
    lcm_set_str(a, "home", hp);
    free(hp);
    json_t *r = lcm_call(s, "read_yaml_layers", a);
    if (lcm_is_raise(r)) {
        if (lcm_is_base_exc(lcm_raise_class(r))) return r;
        json_free(r);
        json_t *out = json_array();
        json_append(out, json_bool(false));
        json_append(out, json_bool(false));
        return out;
    }
    if (!is_obj(r)) {
        /* live .get("plugins") on a non-dict raises AttributeError -> caught */
        json_free(r);
        json_t *out = json_array();
        json_append(out, json_bool(false));
        json_append(out, json_bool(false));
        return out;
    }
    json_t *plugins = json_obj_get(r, "plugins");
    if (!is_obj(plugins)) plugins = NULL;
    bool res[2];
    const char *keys[2] = {"enabled", "disabled"};
    for (int k = 0; k < 2; k++) {
        json_t *entries = plugins ? json_obj_get(plugins, keys[k]) : NULL;
        bool b = false;
        if (is_arr(entries)) {
            for (size_t i = 0; i < entries->c.count; i++) {
                char *ns = lcm_py_str(entries->c.items[i]);
                if (lcm_str_in(ns, names, n_names)) b = true;
                free(ns);
                if (b) break;
            }
        }
        res[k] = b;
    }
    json_free(r);
    json_t *out = json_array();
    json_append(out, json_bool(res[0]));
    json_append(out, json_bool(res[1]));
    return out;
}

/* PoP: lcm_plugin_lists @ hermes_cli/left_core_migration.py:_plugin_lists */
static json_t *lcm_plugin_lists(const lcm_seams_t *s, const char *home, const char *plugin,
                                const char *const *also, size_t n_also) {
    const lcm_feature_t *f = lcm_feature(plugin);
    if (!f) return lcm_stop_iteration();
    lcm_sv_t sv; lcm_sv_init(&sv);
    lcm_sv_push(&sv, f->plugin_key);
    for (size_t i = 0; i < f->n_plugin_names; i++) lcm_sv_push(&sv, f->plugin_names[i]);
    lcm_sv_push(&sv, f->plugin);
    for (size_t i = 0; i < n_also; i++) lcm_sv_push(&sv, also[i]);
    json_t *r = lcm_plugins_listed(s, home, (const char *const *)sv.v, sv.n);
    lcm_sv_free(&sv);
    return r;
}

/* PoP: lcm_bundled_plugin_selected @ hermes_cli/left_core_migration.py:_bundled_plugin_selected */
static json_t *lcm_bundled_plugin_selected(const lcm_seams_t *s, const char *home, const char *plugin) {
    json_t *lists = lcm_plugin_lists(s, home, plugin, NULL, 0);
    if (lcm_is_raise(lists)) return lists;
    bool enabled = jtruth(json_get(lists, 0));
    bool disabled = jtruth(json_get(lists, 1));
    json_free(lists);
    return json_bool(enabled && !disabled);
}

/* PoP: lcm_langfuse_in_use @ hermes_cli/left_core_migration.py:langfuse_in_use */
static json_t *lcm_langfuse_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    (void)process_env;
    return lcm_bundled_plugin_selected(s, home, "langfuse");
}

/* PoP: lcm_spotify_in_use @ hermes_cli/left_core_migration.py:spotify_in_use */
static json_t *lcm_spotify_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    (void)process_env;
    json_t *store = lcm_auth_store(s, home);
    if (lcm_is_raise(store)) return store;
    if (!is_obj(store)) {
        json_t *r = lcm_attr_raise(store, "get");
        json_free(store);
        return r;
    }
    json_t *providers = json_obj_get(store, "providers");
    json_t *state = is_obj(providers) ? json_obj_get(providers, "spotify") : NULL;
    bool has = false;
    if (is_obj(state)) {
        has = jtruth(json_obj_get(state, "access_token")) ||
              jtruth(json_obj_get(state, "refresh_token"));
    }
    json_free(store);
    if (has) return json_bool(true);
    json_t *cfg = lcm_read_config(s, home);
    if (lcm_is_raise(cfg)) return cfg;
    json_t *names = json_array();
    json_append(names, json_string("spotify"));
    json_t *a = json_object();
    json_set(a, "config", cfg);
    json_set(a, "names", names);
    lcm_set_str(a, "own_platform", "");
    return lcm_call(s, "toolset_listed", a);
}

/* PoP: lcm_home_env @ hermes_cli/left_core_migration.py:_home_env */
static json_t *lcm_home_env(const lcm_seams_t *s, const char *home, const char *name,
                            bool process_env) {
    char *hp = lcm_path_posix(home);
    const char *nm = name ? name : "";
    json_t *da = json_object();
    lcm_set_str(da, "home", hp);
    json_t *dotenv = lcm_call(s, "resolve_home_dotenv", da);
    if (lcm_is_raise(dotenv)) { free(hp); return dotenv; }
    if (!is_obj(dotenv)) {
        json_t *r = lcm_attr_raise(dotenv, "get");
        json_free(dotenv); free(hp);
        return r;
    }
    char *value;
    if (!json_has(dotenv, nm)) {
        value = strdup("");
    } else {
        json_t *vn = json_obj_get(dotenv, nm);
        if (!is_str(vn)) {
            json_t *r = lcm_attr_raise(vn, "strip");
            json_free(dotenv); free(hp);
            return r;
        }
        value = lcm_strip(vn->str_val);
    }
    json_free(dotenv);
    /* scope rung */
    json_t *sh = lcm_call(s, "current_secret_scope_home", lcm_args0());
    if (lcm_is_raise(sh)) { free(value); free(hp); return sh; }
    if (!value[0] && jtruth(sh)) {
        json_t *k1a = json_object();
        lcm_set_str(k1a, "home", jstr(sh));
        json_t *k1 = lcm_call(s, "hermes_home_key", k1a);
        if (lcm_is_raise(k1)) { json_free(sh); free(value); free(hp); return k1; }
        json_t *k2a = json_object();
        lcm_set_str(k2a, "home", hp);
        json_t *k2 = lcm_call(s, "hermes_home_key", k2a);
        if (lcm_is_raise(k2)) { json_free(k1); json_free(sh); free(value); free(hp); return k2; }
        bool same = (jstr(k1) && jstr(k2)) ? strcmp(jstr(k1), jstr(k2)) == 0 : false;
        json_free(k1); json_free(k2);
        if (same) {
            json_t *scope = lcm_call(s, "current_secret_scope", lcm_args0());
            if (lcm_is_raise(scope)) { json_free(sh); free(value); free(hp); return scope; }
            json_t *sv = is_obj(scope) ? json_obj_get(scope, nm) : NULL;
            char *raw = jtruth(sv) ? lcm_py_str(sv) : strdup("");
            free(value);
            value = lcm_strip(raw);
            free(raw);
            json_free(scope);
        }
    }
    json_free(sh);
    /* process-env rung */
    if (!value[0] && process_env) {
        json_t *k1a = json_object();
        lcm_set_str(k1a, "home", hp);
        json_t *k1 = lcm_call(s, "hermes_home_key", k1a);
        if (lcm_is_raise(k1)) { free(value); free(hp); return k1; }
        json_t *routing = lcm_call(s, "get_routing_process_hermes_home", lcm_args0());
        if (lcm_is_raise(routing)) { json_free(k1); free(value); free(hp); return routing; }
        char *rp = lcm_path_posix(jstr(routing));
        json_free(routing);
        json_t *k2a = json_object();
        lcm_set_str(k2a, "home", rp);
        free(rp);
        json_t *k2 = lcm_call(s, "hermes_home_key", k2a);
        if (lcm_is_raise(k2)) { json_free(k1); free(value); free(hp); return k2; }
        bool same = (jstr(k1) && jstr(k2)) ? strcmp(jstr(k1), jstr(k2)) == 0 : false;
        json_free(k1); json_free(k2);
        if (same) {
            json_t *ea = json_object();
            lcm_set_str(ea, "name", nm);
            json_t *ev = lcm_call(s, "os_environ_get", ea);
            if (lcm_is_raise(ev)) { free(value); free(hp); return ev; }
            if (!is_str(ev)) {
                json_t *r = lcm_attr_raise(ev, "strip");
                json_free(ev); free(value); free(hp);
                return r;
            }
            free(value);
            value = lcm_strip(ev->str_val);
            json_free(ev);
        }
    }
    free(hp);
    json_t *r = json_string(value);
    free(value);
    return r;
}

/* PoP: lcm_camofox_in_use @ hermes_cli/left_core_migration.py:camofox_in_use */
static json_t *lcm_camofox_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    json_t *cfg = lcm_read_config(s, home);
    if (lcm_is_raise(cfg)) return cfg;
    json_t *browser = json_obj_get(cfg, "browser");
    if (!is_obj(browser)) browser = NULL;
    json_t *cdp = browser ? json_obj_get(browser, "cdp_url") : NULL;
    char *cdps = lcm_strip_or_empty(cdp);
    if (cdps[0]) { free(cdps); json_free(cfg); return json_bool(false); }
    free(cdps);
    json_t *he = lcm_home_env(s, home, "BROWSER_CDP_URL", process_env);
    if (lcm_is_raise(he)) { json_free(cfg); return he; }
    if (jtruth(he)) { json_free(he); json_free(cfg); return json_bool(false); }
    json_free(he);
    json_t *ug = browser ? json_obj_get(browser, "use_gateway") : NULL;
    json_t *ua = json_object();
    lcm_set_node(ua, "value", ug);
    json_t *tv = lcm_call(s, "is_truthy_value", ua);
    if (lcm_is_raise(tv)) { json_free(cfg); return tv; }
    if (jtruth(tv)) { json_free(tv); json_free(cfg); return json_bool(false); }
    json_free(tv);
    json_t *provider = browser ? json_obj_get(browser, "cloud_provider") : NULL;
    char *selected;
    if (jpresent(provider)) {
        char *raw = lcm_py_str(provider);
        char *st = lcm_strip(raw); free(raw);
        selected = lcm_lower(st); free(st);
    } else {
        selected = strdup("");
    }
    json_free(cfg);
    if (selected[0]) {
        bool b = strcmp(selected, "camofox") == 0;
        free(selected);
        return json_bool(b);
    }
    free(selected);
    json_t *he2 = lcm_home_env(s, home, "CAMOFOX_URL", process_env);
    if (lcm_is_raise(he2)) return he2;
    bool b = jtruth(he2);
    json_free(he2);
    return json_bool(b);
}

/* PoP: lcm_singularity_in_use @ hermes_cli/left_core_migration.py:singularity_in_use */
static json_t *lcm_singularity_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    char *hp = lcm_path_posix(home);
    json_t *ya = json_object();
    lcm_set_str(ya, "home", hp);
    json_t *r = lcm_call(s, "read_yaml_layers", ya);
    json_t *terminal = NULL;
    if (lcm_is_raise(r)) {
        if (lcm_is_base_exc(lcm_raise_class(r))) { free(hp); return r; }
        json_free(r); r = NULL;
    } else if (is_obj(r)) {
        terminal = json_obj_get(r, "terminal");
        if (!is_obj(terminal)) terminal = NULL;
    } else {
        json_free(r); r = NULL;
    }
    json_t *backend = terminal ? json_obj_get(terminal, "backend") : NULL;
    json_t *env_type = terminal ? json_obj_get(terminal, "env_type") : NULL;
    const json_t *pick = jtruth(backend) ? backend : (jtruth(env_type) ? env_type : NULL);
    char *configured = lcm_strip_or_empty(pick);
    json_free(r);
    if (!configured[0]) {
        json_t *da = json_object();
        lcm_set_str(da, "home", hp);
        json_t *dotenv = lcm_call(s, "resolve_home_dotenv", da);
        if (lcm_is_raise(dotenv)) { free(configured); free(hp); return dotenv; }
        if (!is_obj(dotenv)) {
            json_t *rr = lcm_attr_raise(dotenv, "get");
            json_free(dotenv); free(configured); free(hp);
            return rr;
        }
        char *nv;
        if (!json_has(dotenv, "TERMINAL_ENV")) {
            nv = strdup("");
        } else {
            json_t *te = json_obj_get(dotenv, "TERMINAL_ENV");
            if (!is_str(te)) {
                json_t *rr = lcm_attr_raise(te, "strip");
                json_free(dotenv); free(configured); free(hp);
                return rr;
            }
            nv = lcm_strip(te->str_val);
        }
        json_free(dotenv);
        free(configured);
        configured = nv;
    }
    if (!configured[0] && process_env) {
        json_t *k1a = json_object();
        lcm_set_str(k1a, "home", hp);
        json_t *k1 = lcm_call(s, "hermes_home_key", k1a);
        if (lcm_is_raise(k1)) { free(configured); free(hp); return k1; }
        json_t *routing = lcm_call(s, "get_routing_process_hermes_home", lcm_args0());
        if (lcm_is_raise(routing)) { json_free(k1); free(configured); free(hp); return routing; }
        char *rp = lcm_path_posix(jstr(routing));
        json_free(routing);
        json_t *k2a = json_object();
        lcm_set_str(k2a, "home", rp);
        free(rp);
        json_t *k2 = lcm_call(s, "hermes_home_key", k2a);
        if (lcm_is_raise(k2)) { json_free(k1); free(configured); free(hp); return k2; }
        bool same = (jstr(k1) && jstr(k2)) ? strcmp(jstr(k1), jstr(k2)) == 0 : false;
        json_free(k1); json_free(k2);
        if (same) {
            json_t *ea = json_object();
            lcm_set_str(ea, "name", "TERMINAL_ENV");
            json_t *ev = lcm_call(s, "os_environ_get", ea);
            if (lcm_is_raise(ev)) { free(configured); free(hp); return ev; }
            if (!is_str(ev)) {
                json_t *rr = lcm_attr_raise(ev, "strip");
                json_free(ev); free(configured); free(hp);
                return rr;
            }
            free(configured);
            configured = lcm_strip(ev->str_val);
            json_free(ev);
        }
    }
    free(hp);
    char *low = lcm_lower(configured);
    free(configured);
    bool b = strcmp(low, "singularity") == 0;
    free(low);
    return json_bool(b);
}

/* forward decl */
static json_t *lcm_platform_configured_on(const lcm_seams_t *s, const char *home, const char *name);

/* PoP: lcm_homeassistant_in_use @ hermes_cli/left_core_migration.py:homeassistant_in_use */
static json_t *lcm_homeassistant_in_use(const lcm_seams_t *s, const char *home, bool process_env) {
    char *hp = lcm_path_posix(home);
    char *envpath = lcm_path_join(hp, ".env");
    json_t *ea = json_object();
    lcm_set_str(ea, "path", envpath);
    free(envpath);
    json_t *ef = lcm_call(s, "load_env_file", ea);
    if (lcm_is_raise(ef)) { free(hp); return ef; }
    if (!is_obj(ef)) {
        json_t *r = lcm_attr_raise(ef, "get");
        json_free(ef); free(hp);
        return r;
    }
    json_t *hass = json_obj_get(ef, "HASS_TOKEN");
    char *v = lcm_strip_or_empty(hass);
    json_free(ef);
    if (v[0]) { free(v); free(hp); return json_bool(true); }
    free(v);
    if (process_env) {
        json_t *ga = json_object();
        lcm_set_str(ga, "name", "HASS_TOKEN");
        lcm_set_str(ga, "default", "");
        json_t *gs = lcm_call(s, "get_secret", ga);
        if (lcm_is_raise(gs)) {
            if (lcm_is_base_exc(lcm_raise_class(gs))) { free(hp); return gs; }
            json_free(gs);
        } else {
            /* (get_secret(...) or "").strip() — a truthy non-str raises .strip
             * AttributeError, caught by the surrounding except Exception. */
            bool ok = true;
            char *gv = NULL;
            if (jtruth(gs) && !is_str(gs)) {
                ok = false;
            } else {
                gv = lcm_strip_or_empty(gs);
            }
            json_free(gs);
            if (ok && gv && gv[0]) { free(gv); free(hp); return json_bool(true); }
            free(gv);
        }
    }
    json_t *pc = lcm_platform_configured_on(s, hp, "homeassistant");
    if (lcm_is_raise(pc)) { free(hp); return pc; }
    if (jtruth(pc)) { json_free(pc); free(hp); return json_bool(true); }
    json_free(pc);
    json_t *cfg = lcm_read_config(s, hp);
    free(hp);
    if (lcm_is_raise(cfg)) return cfg;
    json_t *names = json_array();
    json_append(names, json_string("hermes-homeassistant"));
    json_append(names, json_string("homeassistant"));
    json_t *a = json_object();
    json_set(a, "config", cfg);
    json_set(a, "names", names);
    lcm_set_str(a, "own_platform", "homeassistant");
    return lcm_call(s, "toolset_listed", a);
}

/* PoP: lcm_platform_config @ hermes_cli/left_core_migration.py:_platform_config */
/* Returns the canonical {enabled,token,extra} desc (defaults false/null/{}). */
static json_t *lcm_platform_config(const lcm_seams_t *s, const json_t *block) {
    json_t *data = is_obj(block) ? json_copy(block) : json_object();
    json_set(data, "home_channel", json_null());
    json_t *a = json_object();
    json_set(a, "block", data);
    json_t *r = lcm_call(s, "platform_config_from_dict", a);
    if (lcm_is_raise(r)) return r;
    json_t *desc = json_object();
    json_set(desc, "enabled", json_bool(jtruth(json_obj_get(r, "enabled"))));
    json_t *tok = json_obj_get(r, "token");
    json_set(desc, "token", jpresent(tok) ? json_copy(tok) : json_null());
    json_t *extra = json_obj_get(r, "extra");
    json_set(desc, "extra", is_obj(extra) ? json_copy(extra) : json_object());
    json_free(r);
    return desc;
}

/* PoP: lcm_platform_configured_on @ hermes_cli/left_core_migration.py:platform_configured_on */
static json_t *lcm_platform_configured_on(const lcm_seams_t *s, const char *home, const char *name) {
    char *hp = lcm_path_posix(home);
    json_t *a = json_object();
    lcm_set_str(a, "home", hp);
    lcm_set_str(a, "name", name ? name : "");
    free(hp);
    json_t *block = lcm_call(s, "merged_platform_block", a);
    if (lcm_is_raise(block)) return block;
    if (!jpresent(block)) { json_free(block); return json_bool(false); }
    json_t *pc = lcm_platform_config(s, block);
    if (lcm_is_raise(pc)) { json_free(block); return pc; }
    bool enabled = jtruth(json_obj_get(pc, "enabled"));
    json_t *token = json_obj_get(pc, "token");
    char *ts = lcm_strip_or_empty(token);
    bool has_enabled = json_has(block, "enabled");
    bool b = enabled || (!has_enabled && ts[0]);
    free(ts);
    json_free(pc);
    json_free(block);
    return json_bool(b);
}

/* PoP: lcm_yuanbao_notice @ hermes_cli/left_core_migration.py:_yuanbao_notice */
static json_t *lcm_yuanbao_notice(const lcm_seams_t *s, const char *home, bool process_env) {
    (void)process_env;
    json_t *pres = lcm_plugin_present(s, "yuanbao", home);
    if (lcm_is_raise(pres)) return pres;
    bool present = jtruth(pres);
    json_free(pres);
    if (present) {
        const char *names[] = {"yuanbao"};
        json_t *lists = lcm_plugins_listed(s, home, names, 1);
        if (lcm_is_raise(lists)) return lists;
        bool disabled = jtruth(json_get(lists, 1));
        json_free(lists);
        if (disabled)
            return json_string(LCM_YUANBAO_NOTICE
                " plugins.disabled lists 'yuanbao', which now turns Yuanbao off; "
                "remove it to keep using Yuanbao.");
    }
    return json_string(LCM_YUANBAO_NOTICE);
}

/* ═══════════════════════ notice / scope leaves ═══════════════════════ */

/* PoP: lcm_notice_file @ hermes_cli/left_core_migration.py:_notice_file */
static json_t *lcm_notice_file(const char *home) {
    char *hp = lcm_path_posix(home);
    char *p1 = lcm_path_join(hp, "cache");
    free(hp);
    char *path = lcm_path_join(p1, "left-core-notices.jsonl");
    free(p1);
    json_t *r = json_string(path);
    free(path);
    return r;
}

/* PoP: lcm_scope_kept @ hermes_cli/left_core_migration.py:_scope_kept */
static json_t *lcm_scope_kept(const lcm_seams_t *s, const json_t *config, const char *feature_plugin) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    if (f->n_toolsets == 0) return json_bool(true);
    json_t *a = json_object();
    lcm_set_node(a, "config", config);
    lcm_set_str(a, "key", "_left_core_scoped");
    lcm_set_str(a, "plugin", f->plugin);
    return lcm_call(s, "marked", a);
}

/* PoP: lcm_mark @ hermes_cli/left_core_migration.py:_mark */
/* Mutates config in place; returns NULL (None) or a StopIteration raise. */
static json_t *lcm_mark(json_t *config, const char *key, const char *feature_plugin) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    if (!is_obj(config)) return NULL;
    json_t *done = json_obj_get(config, key);
    lcm_sv_t sv; lcm_sv_init(&sv);
    if (is_arr(done)) {
        for (size_t i = 0; i < done->c.count; i++) {
            char *s = lcm_py_str(done->c.items[i]);
            lcm_sv_push(&sv, s);
            free(s);
        }
    }
    lcm_sv_push(&sv, f->plugin);
    json_t *arr = lcm_sorted_set(&sv);
    lcm_sv_free(&sv);
    json_set(config, key, arr);
    return NULL;
}

/* PoP: lcm_defer_startup_notice @ hermes_cli/left_core_migration.py:defer_startup_notice */
static json_t *lcm_defer_startup_notice(const lcm_seams_t *s, const char *message, bool log) {
    const char *msg = message ? message : "";
    if (log) {
        json_t *a = json_object();
        lcm_set_str(a, "message", msg);
        json_t *r = lcm_call(s, "logger_warning", a);
        if (lcm_is_raise(r)) return r;
        json_free(r);
    }
    json_t *hr = lcm_call(s, "get_hermes_home", lcm_args0());
    if (lcm_is_raise(hr)) return hr;
    char *home = lcm_path_posix(jstr(hr));
    json_free(hr);
    json_t *a = json_object();
    lcm_set_str(a, "home", home);
    lcm_set_str(a, "message", msg);
    free(home);
    return lcm_call(s, "queue_notice", a);
}

/* PoP: lcm_say_post_split @ hermes_cli/left_core_migration.py:_say_post_split */
static json_t *lcm_say_post_split(const lcm_seams_t *s, const char *home, const char *feature_plugin,
                                  bool backoff, bool process_env) {
    const lcm_feature_t *f = lcm_feature(feature_plugin);
    if (!f) return lcm_stop_iteration();
    char *hp = lcm_path_posix(home);
    json_t *pres = lcm_plugin_present(s, f->plugin, hp);
    if (lcm_is_raise(pres)) { free(hp); return pres; }
    if (jtruth(pres)) { json_free(pres); free(hp); return NULL; }
    json_free(pres);
    if (backoff) {
        json_t *fa = json_object();
        lcm_set_str(fa, "home", hp);
        lcm_set_str(fa, "plugin", f->plugin);
        json_t *fr = lcm_call(s, "failed_recently", fa);
        if (lcm_is_raise(fr)) { free(hp); return fr; }
        if (jtruth(fr)) { json_free(fr); free(hp); return NULL; }
        json_free(fr);
    }
    json_t *na = json_object();
    lcm_set_str(na, "home", hp);
    lcm_set_str(na, "plugin", f->plugin);
    json_t *nf = lcm_call(s, "note_failure", na);
    if (lcm_is_raise(nf)) { free(hp); return nf; }
    json_free(nf);
    json_t *ca = json_object();
    lcm_set_str(ca, "plugin", f->plugin);
    lcm_set_str(ca, "home", hp);
    json_t *cmd = lcm_call(s, "install_command", ca);
    if (lcm_is_raise(cmd)) { free(hp); return cmd; }
    char *cs = lcm_py_str(cmd);
    json_free(cmd);
    char *msg = xsf("  \xe2\x9a\xa0 %s moved out of core into the '%s' plugin, which this "
                    "profile does not have, so it is off. Run `%s`.",
                    f->label, f->plugin, cs);
    free(cs);
    json_t *noted = lcm_noticed(s, msg, hp, f->plugin, process_env);
    free(hp);
    free(msg);
    if (lcm_is_raise(noted)) return noted;
    char *ns = lcm_py_str(noted);
    json_free(noted);
    json_t *sa = json_object();
    lcm_set_str(sa, "message", ns);
    free(ns);
    json_t *sr = lcm_call(s, "say", sa);
    if (lcm_is_raise(sr)) return sr;
    json_free(sr);
    return NULL;
}