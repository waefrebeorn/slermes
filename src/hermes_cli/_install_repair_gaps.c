#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_install_repair.py: 8 missing functions ── */

/* ── rest_revive1 seam infrastructure (_install_repair_gaps) ────────────────
 * json_t *fn(ir_io_t *io, json_t *in): io->call is the injected I/O seam.
 * ir_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception. NULL ==
 * None. Integral JSON numbers model Python ints.
 *
 * winreg model: "import_winreg" is the import seam; "winreg.OpenKey" returns
 * a scripted handle (the with-statement enter/exit are silent);
 * "winreg.QueryValueEx" returns the [raw, kind] pair. REG_EXPAND_SZ is the
 * live winreg constant 2 (probed). str.strip() is Unicode-whitespace-aware;
 * case folding in normalize_windows_path is ASCII-only (PATH comparison
 * literals are ASCII; documented in the oracle).
 *
 * Production default (no seam channel installed) is fail-closed. */
typedef struct ir_io {
    json_t *(*call)(struct ir_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ir_io_t;

static int ir_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ir_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

/* except (OSError, ImportError) catch set (common builtin subclasses). */
static int ir_caught_kind(const json_t *r) {
    if (!ir_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    static const char *const kinds[] = {
        "OSError", "FileNotFoundError", "PermissionError", "FileExistsError",
        "IsADirectoryError", "NotADirectoryError", "TimeoutError",
        "InterruptedError", "ImportError", "ModuleNotFoundError",
    };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (strcmp(k, kinds[i]) == 0) return 1;
    return 0;
}

static int ir_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *ir_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *ir_seam(ir_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return ir_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static unsigned ir_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static int ir_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

static char *ir_strip_dup(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start) {
        const char *n = start;
        unsigned cp = ir_cp_next(&n);
        if (!ir_isspace_cp(cp)) break;
        start = n;
    }
    const char *end = s + strlen(s);
    while (end > start) {
        const char *p = start;
        const char *last = p;
        unsigned cp = 0;
        while (*p) { last = p; cp = ir_cp_next(&p); }
        if (!ir_isspace_cp(cp)) break;
        end = last;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* str(v): None -> "None", str passthrough, integral numbers -> decimal,
 * bool -> True/False (fixtures restrict to these types). */
static char *ir_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    char *out = strdup(ser ? ser : "");
    free(ser);
    return out;
}

/* int(v) for the registry-kind slot: numbers truncate toward zero, bool ->
 * 0/1, strings parse with Python int() rules (whitespace strip, sign,
 * underscores BETWEEN digits). Returns a number node or a raise spec. */
static json_t *ir_py_int(const json_t *v) {
    if (v && v->type == JSON_NUMBER)
        return json_number((double)(long long)v->num_val);
    if (v && v->type == JSON_BOOL)
        return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        char *s = ir_strip_dup(v->str_val);
        const char *p = s;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        const char *digits = p;
        int ndigit = 0, prev_digit = 0, ok = 1;
        for (; *p; p++) {
            if (*p >= '0' && *p <= '9') { ndigit++; prev_digit = 1; }
            else if (*p == '_') {
                if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = 0; break; }
                prev_digit = 0;
            } else { ok = 0; break; }
        }
        if (!ok || ndigit == 0) {
            char *msg = malloc(strlen(v->str_val) + 80);
            snprintf(msg, strlen(v->str_val) + 80,
                "invalid literal for int() with base 10: '%s'", v->str_val);
            json_t *e = ir_raise("ValueError", msg);
            free(msg);
            free(s);
            return e;
        }
        char *clean = malloc(strlen(digits) + 1);
        size_t k = 0;
        for (const char *q = digits; *q; q++)
            if (*q != '_') clean[k++] = *q;
        clean[k] = '\0';
        long long val = strtoll(clean, NULL, 10);
        free(clean);
        free(s);
        return json_number((double)(neg ? -val : val));
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        ir_typename(v));
    return ir_raise("TypeError", msg);
}

/* Unpack a 2-tuple seam result ([raw, kind]); non-2 arrays raise the exact
 * py3.14 unpack messages, non-iterables raise TypeError. Returns 0 on
 * success (out nodes borrowed), else a raise spec the caller propagates. */
static json_t *ir_unpack2(json_t *r, json_t **a, json_t **b) {
    if (r && r->type == JSON_ARRAY) {
        if (r->c.count == 2) {
            *a = r->c.items[0];
            *b = r->c.items[1];
            return NULL;
        }
        char msg[96];
        if (r->c.count < 2)
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", r->c.count);
        else
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", r->c.count);
        return ir_raise("ValueError", msg);
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", ir_typename(r));
    return ir_raise("TypeError", msg);
}

/* Split value on ';' into a JSON array; keep_strip != 0 filters entries
 * whose .strip() is empty (windows_user_path_entries), else only the empty
 * string is dropped (read_user_path_raw). */
static json_t *ir_split_path(const char *value, int keep_strip) {
    json_t *out = json_array();
    const char *p = value;
    for (;;) {
        const char *semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        char *entry = malloc(len + 1);
        memcpy(entry, p, len);
        entry[len] = '\0';
        int keep;
        if (keep_strip) {
            char *st = ir_strip_dup(entry);
            keep = st && st[0];
            free(st);
        } else {
            keep = entry[0] != '\0';
        }
        if (keep) json_append(out, json_string(entry));
        free(entry);
        if (!semi) break;
        p = semi + 1;
    }
    return out;
}

/* PoP: venv_is_relocatable @ hermes_cli/_install_repair.py:venv_is_relocatable */
int venv_is_relocatable(void) { return 0; }

/* PoP: normalize_windows_path @ hermes_cli/_install_repair.py:_normalize_windows_path */
json_t *normalize_windows_path(ir_io_t *io, json_t *in) {
    (void)io;
    json_t *value = json_obj_get(in, "value");
    char *s = ir_py_str(value);
    for (char *p = s; *p; p++) {
        if (*p == '/') *p = '\\';
        else if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    }
    size_t n = strlen(s);
    while (n && s[n - 1] == '\\') n--;
    s[n] = '\0';
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: windows_user_path_entries @ hermes_cli/_install_repair.py:_windows_user_path_entries */
json_t *windows_user_path_entries(ir_io_t *io, json_t *in) {
    (void)in;
    /* try: import winreg; with OpenKey(HKCU, "Environment") as key:
     *          raw, _kind = QueryValueEx(key, "Path")
     *      value = expandvars(str(raw))
     * except (OSError, ImportError): value = os.environ.get("PATH", "") */
    char *value = NULL;
    json_t *imp = ir_seam(io, "import_winreg", NULL);
    if (ir_is_raise(imp)) {
        if (!ir_caught_kind(imp)) return imp;
        json_free(imp);
    } else {
        json_free(imp);
        json_t *oargs = json_object();
        json_set_hdr(oargs, "hive", "HKEY_CURRENT_USER");
        json_set_hdr(oargs, "key", "Environment");
        json_t *handle = ir_seam(io, "winreg.OpenKey", oargs);
        if (ir_is_raise(handle)) {
            if (!ir_caught_kind(handle)) return handle;
            json_free(handle);
        } else {
            json_t *qargs = json_object();
            json_set(qargs, "key", handle); /* adopts the handle node */
            json_set_hdr(qargs, "name", "Path");
            json_t *pair = ir_seam(io, "winreg.QueryValueEx", qargs);
            if (ir_is_raise(pair)) {
                if (!ir_caught_kind(pair)) return pair;
                json_free(pair);
            } else {
                json_t *raw = NULL, *kind = NULL;
                json_t *uerr = ir_unpack2(pair, &raw, &kind);
                if (uerr) { json_free(pair); return uerr; }
                char *raw_s = ir_py_str(raw);
                json_t *eargs = json_object();
                json_set_hdr(eargs, "value", raw_s);
                free(raw_s);
                json_t *expanded = ir_seam(io, "os.path.expandvars", eargs);
                json_free(pair);
                if (ir_is_raise(expanded)) {
                    if (!ir_caught_kind(expanded)) return expanded;
                    json_free(expanded);
                } else if (expanded && expanded->type == JSON_STRING) {
                    value = strdup(expanded->str_val);
                    json_free(expanded);
                } else {
                    /* expandvars returned a non-str: live would hand it to
                     * .split — AttributeError, NOT caught by (OSError,
                     * ImportError). Fixtures script strings only. */
                    char m[160];
                    snprintf(m, sizeof m, "'%s' object has no attribute 'split'", ir_typename(expanded));
                    json_t *e = ir_raise("AttributeError", m);
                    if (expanded) json_free(expanded);
                    return e;
                }
            }
        }
    }
    if (!value) {
        json_t *fargs = json_object();
        json_set_hdr(fargs, "key", "PATH");
        json_set_hdr(fargs, "default", "");
        json_t *fv = ir_seam(io, "os.environ.get", fargs);
        if (ir_is_raise(fv)) return fv;
        value = (fv && fv->type == JSON_STRING) ? strdup(fv->str_val) : strdup("");
        if (fv) json_free(fv);
    }
    json_t *out = ir_split_path(value, 1);
    free(value);
    return out;
}

/* PoP: read_user_path_raw @ hermes_cli/_install_repair.py:_read_user_path_raw */
json_t *read_user_path_raw(ir_io_t *io, json_t *in) {
    (void)in;
    /* import winreg — NOT guarded: ImportError propagates. */
    json_t *imp = ir_seam(io, "import_winreg", NULL);
    if (ir_is_raise(imp)) return imp;
    json_free(imp);
    json_t *oargs = json_object();
    json_set_hdr(oargs, "hive", "HKEY_CURRENT_USER");
    json_set_hdr(oargs, "key", "Environment");
    json_t *handle = ir_seam(io, "winreg.OpenKey", oargs);
    if (ir_is_raise(handle)) return handle;
    json_t *qargs = json_object();
    json_set(qargs, "key", handle);
    json_set_hdr(qargs, "name", "Path");
    json_t *pair = ir_seam(io, "winreg.QueryValueEx", qargs);
    if (ir_is_raise(pair)) {
        const char *k = json_get_str(pair, "__raise__", "");
        if (strcmp(k, "FileNotFoundError") == 0) {
            json_free(pair);
            /* return [], winreg.REG_EXPAND_SZ (== 2) */
            json_t *out = json_array();
            json_append(out, json_array());
            json_append(out, json_number(2));
            return out;
        }
        return pair;
    }
    json_t *raw = NULL, *kind = NULL;
    json_t *uerr = ir_unpack2(pair, &raw, &kind);
    if (uerr) { json_free(pair); return uerr; }
    char *raw_s = ir_py_str(raw);
    json_t *entries = ir_split_path(raw_s, 0);
    free(raw_s);
    json_t *kind_int = ir_py_int(kind);
    json_free(pair);
    if (ir_is_raise(kind_int)) { json_free(entries); return kind_int; }
    json_t *out = json_array();
    json_append(out, entries);
    json_append(out, kind_int);
    return out;
}
