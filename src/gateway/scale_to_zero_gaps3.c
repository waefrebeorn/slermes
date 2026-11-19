#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/scale_to_zero.py: 3 missing functions ── */

/* ── rest_revive1 seam infrastructure (scale_to_zero_gaps3) ─────────────────
 * json_t *fn(sz_io_t *io, json_t *in): io->call is the injected I/O seam.
 * sz_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception; except
 * Exception sites swallow plain specs but pass the BaseException trio
 * through. NULL == None. Integral JSON numbers model Python ints,
 * fractional ones floats.
 *
 * Path model: DASHBOARD_CLIENT_HEARTBEAT_REL is os.path.join("state",
 * "dashboard_clients.heartbeat"); Path(home) / REL is projected to a POSIX
 * string (the oracle norm()s live Path returns with as_posix()).
 * sz_path_norm reproduces Windows pathlib: backslashes fold to '/', the
 * LEADING separator run is preserved verbatim, '.' and empty components
 * drop, internal runs collapse, trailing separators drop. Path("") is ".".
 * Non-str home raises the py3.14 Path() TypeError (probed message).
 *
 * touch: open(p, "a").close() is modeled as the "open" seam followed by the
 * "close" seam (file.close()); os.path.dirname is a seam (platform-dependent
 * in live). last_seen: os.stat returns {"st_mtime": ...}; FileNotFoundError
 * -> None, other OSError -> now (fail-awake), result clamped to now.
 *
 * Production default (no seam channel installed) is fail-closed. */
typedef struct sz_io {
    json_t *(*call)(struct sz_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sz_io_t;

static int sz_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int sz_is_base_exc(const json_t *r) {
    if (!sz_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0;
}

static int sz_is_oserror(const json_t *r) {
    if (!sz_is_raise(r)) return 0;
    const char *k = json_get_str(r, "__raise__", "");
    static const char *const kinds[] = {
        "OSError", "FileNotFoundError", "PermissionError", "FileExistsError",
        "IsADirectoryError", "NotADirectoryError", "TimeoutError", "InterruptedError",
    };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (strcmp(k, kinds[i]) == 0) return 1;
    return 0;
}

static json_t *sz_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static const char *sz_typename(const json_t *v) {
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

static json_t *sz_seam(sz_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return sz_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *sz_path_norm(const char *s) {
    size_t n = strlen(s);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    for (size_t i = 0; i < n; i++) tmp[i] = (s[i] == '\\') ? '/' : s[i];
    tmp[n] = '\0';
    size_t lead = 0;
    while (tmp[lead] == '/') lead++;
    char *out = malloc(n + 2);
    if (!out) { free(tmp); return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < lead; i++) out[k++] = '/';
    char *p = tmp + lead;
    int have_comp = 0;
    while (*p) {
        char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* drop */
        } else {
            if (have_comp) out[k++] = '/';
            memcpy(out + k, p, len);
            k += len;
            have_comp = 1;
        }
        if (!slash) break;
        p = slash + 1;
    }
    out[k] = '\0';
    free(tmp);
    return out;
}

/* Path(home) / rel for a fixed relative POSIX tail. */
static char *sz_path_join(const char *home, const char *rel) {
    char *norm = sz_path_norm(home);
    if (!norm) return NULL;
    if (!norm[0] || strcmp(norm, ".") == 0) {
        free(norm);
        return strdup(rel);
    }
    size_t hn = strlen(norm), rn = strlen(rel);
    int need_sep = norm[hn - 1] != '/';
    char *out = malloc(hn + rn + 2);
    if (!out) { free(norm); return NULL; }
    memcpy(out, norm, hn);
    size_t k = hn;
    if (need_sep) out[k++] = '/';
    memcpy(out + k, rel, rn + 1);
    free(norm);
    return out;
}

static json_t *sz_path_type_error(const json_t *v) {
    char m[200];
    snprintf(m, sizeof m,
        "argument should be a str or an os.PathLike object where"
        " __fspath__ returns a str, not '%s'", sz_typename(v));
    return sz_raise("TypeError", m);
}

/* PoP: dashboard_client_heartbeat_path @ gateway/scale_to_zero.py:dashboard_client_heartbeat_path */
json_t *dashboard_client_heartbeat_path(sz_io_t *io, json_t *in) {
    json_t *home = json_obj_get(in, "hermes_home");
    int home_owned = 0;
    if (!home || home->type == JSON_NULL) {
        /* from hermes_constants import get_process_hermes_home */
        json_t *imp = sz_seam(io, "import_get_process_hermes_home", NULL);
        if (sz_is_raise(imp)) return imp;
        json_free(imp);
        home = sz_seam(io, "get_process_hermes_home", NULL);
        home_owned = 1;
        if (sz_is_raise(home)) return home;
    }
    if (!home || home->type != JSON_STRING) {
        json_t *e = sz_path_type_error(home);
        if (home_owned) json_free(home);
        return e;
    }
    char *joined = sz_path_join(home->str_val, "state/dashboard_clients.heartbeat");
    json_t *out = json_string(joined ? joined : "");
    free(joined);
    if (home_owned) json_free(home);
    return out;
}

/* PoP: touch_dashboard_client_heartbeat @ gateway/scale_to_zero.py:touch_dashboard_client_heartbeat */
json_t *touch_dashboard_client_heartbeat(sz_io_t *io, json_t *in) {
    /* Best-effort: every Exception -> logger.debug + False; the
     * BaseException trio propagates. */
    json_t *path = json_obj_get(in, "path");
    json_t *p = NULL;
    json_t *caught = NULL;
    if (path && path->type != JSON_NULL) {
        p = json_copy(path);
    } else {
        p = sz_seam(io, "dashboard_client_heartbeat_path", NULL);
        if (sz_is_raise(p)) {
            if (sz_is_base_exc(p)) return p;
            caught = p;
            p = NULL;
        }
    }
    if (!caught) {
        /* os.makedirs(os.path.dirname(p), exist_ok=True) */
        json_t *dargs = json_object();
        json_set(dargs, "path", json_copy(p));
        json_t *dir = sz_seam(io, "os.path.dirname", dargs);
        if (sz_is_raise(dir)) {
            if (sz_is_base_exc(dir)) { json_free(p); return dir; }
            caught = dir;
        } else {
            json_t *margs = json_object();
            json_set(margs, "path", dir); /* adopts */
            json_set(margs, "exist_ok", json_bool(true));
            json_t *mr = sz_seam(io, "os.makedirs", margs);
            if (sz_is_raise(mr)) {
                if (sz_is_base_exc(mr)) { json_free(p); return mr; }
                caught = mr;
            } else {
                json_free(mr);
                /* open(p, "a", encoding="utf-8").close() */
                json_t *oargs = json_object();
                json_set(oargs, "path", json_copy(p));
                json_set_hdr(oargs, "mode", "a");
                json_set_hdr(oargs, "encoding", "utf-8");
                json_t *fr = sz_seam(io, "open", oargs);
                if (sz_is_raise(fr)) {
                    if (sz_is_base_exc(fr)) { json_free(p); return fr; }
                    caught = fr;
                } else {
                    json_free(fr);
                    json_t *cr = sz_seam(io, "close", NULL);
                    if (sz_is_raise(cr)) {
                        if (sz_is_base_exc(cr)) { json_free(p); return cr; }
                        caught = cr;
                    } else {
                        json_free(cr);
                        /* os.utime(p, None) */
                        json_t *uargs = json_object();
                        json_set(uargs, "path", json_copy(p));
                        json_set(uargs, "times", json_null());
                        json_t *ur = sz_seam(io, "os.utime", uargs);
                        if (sz_is_raise(ur)) {
                            if (sz_is_base_exc(ur)) { json_free(p); return ur; }
                            caught = ur;
                        } else {
                            json_free(ur);
                            json_free(p);
                            return json_bool(true);
                        }
                    }
                }
            }
        }
    }
    /* except Exception: logger.debug(..., exc_info=True); return False */
    json_free(p);
    json_t *largs = json_object();
    json_set_hdr(largs, "msg", "scale-to-zero: dashboard heartbeat touch failed");
    json_set(largs, "exc_info", json_bool(true));
    json_t *lr = sz_seam(io, "logger.debug", largs);
    if (sz_is_raise(lr)) {
        if (caught) json_free(caught);
        return lr;
    }
    json_free(lr);
    if (caught) json_free(caught);
    return json_bool(false);
}

/* PoP: dashboard_client_last_seen @ gateway/scale_to_zero.py:dashboard_client_last_seen */
json_t *dashboard_client_last_seen(sz_io_t *io, json_t *in) {
    /* current = time.time() if now is None else now */
    json_t *now = json_obj_get(in, "now");
    json_t *current;
    if (!now || now->type == JSON_NULL) {
        current = sz_seam(io, "time.time", NULL);
        if (sz_is_raise(current)) return current;
    } else {
        current = json_copy(now);
    }
    /* p = dashboard_client_heartbeat_path() if path is None else path */
    json_t *path = json_obj_get(in, "path");
    json_t *p;
    if (path && path->type != JSON_NULL) {
        p = json_copy(path);
    } else {
        p = sz_seam(io, "dashboard_client_heartbeat_path", NULL);
        if (sz_is_raise(p)) { json_free(current); return p; }
    }
    /* try: return min(os.stat(p).st_mtime, current)
     * except FileNotFoundError: return None
     * except OSError: return current */
    json_t *sargs = json_object();
    json_set(sargs, "path", p); /* adopts */
    json_t *st = sz_seam(io, "os.stat", sargs);
    if (sz_is_raise(st)) {
        const char *k = json_get_str(st, "__raise__", "");
        if (strcmp(k, "FileNotFoundError") == 0) {
            json_free(st);
            json_free(current);
            return json_null();
        }
        if (sz_is_oserror(st)) {
            json_free(st);
            return current;
        }
        json_free(current);
        return st;
    }
    json_t *mtime = (st && st->type == JSON_OBJECT) ? json_obj_get(st, "st_mtime") : NULL;
    if (!mtime || mtime->type != JSON_NUMBER) {
        /* the oracle's os.stat double is a SimpleNamespace attribute carrier */
        json_free(st);
        json_free(current);
        return sz_raise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'st_mtime'");
    }
    if (!current || current->type != JSON_NUMBER) {
        /* min(float, non-number) raises TypeError (probed py3.14 shape). */
        char msg[160];
        snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and 'float'",
            sz_typename(current));
        json_free(st);
        if (current) json_free(current);
        return sz_raise("TypeError", msg);
    }
    double a = mtime->num_val;
    double b = current->num_val;
    json_t *out = json_number(a < b ? a : b);
    json_free(st);
    json_free(current);
    return out;
}
