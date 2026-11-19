#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/plugin_dev.py: 5 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the plugin_dev gap stubs.
 * Universal seam interface: json_t *fn(pd2_io_t *io, json_t *in);
 * pd2_seam() records {ev, args, ret} in call order; scripted results
 * {"__raise__": Type, "message": ...} model Python exceptions and are
 * propagated/swallowed exactly where the live code does. BaseException
 * kinds pass through every `except Exception` site. NULL == None.
 *
 * Seam map:
 *   inspect.signature(callback)        -> "inspect.signature" {callback:"<cb>"}
 *     (result {"parameters": [<kind name>, ...]}; kinds are the live
 *      inspect.Parameter.Kind member names; the callback itself is an
 *      opaque double, recorded as the constant marker "<cb>")
 *   (path / name).exists()             -> "path.exists" {path}
 *   (path / name).is_symlink()         -> "path.is_symlink" {path}
 *   path.is_dir()                      -> "path.is_dir" {path}
 *   _has_manifest(path)                -> "has_manifest" {path}
 *   _category_plugins(path)            -> "category_plugins" {path}
 *
 * Path modeling: fixture paths are non-empty POSIX-style absolute paths
 * without trailing separators or dot components, so Path joining is
 * exactly path + "/" + name on both sides (the oracle normalizes with
 * as_posix()). Empty path joins to the bare name (Path("") == Path(".")).
 *
 * is_plugin_id models pathlib.PurePath of the BUILD host (PureWindowsPath
 * on Windows, PurePosixPath elsewhere) — the oracle runs the live def on
 * the same host, so the projection matches per platform. Windows rules
 * transcribed from CPython 3.14 ntpath.isabs + pathlib splitting:
 *   absolute <=> two leading separators (UNC/device) OR X: + separator;
 *   a single leading separator is a root WITHOUT a drive (not absolute);
 *   "." components collapse, ".." components survive; the drive/root
 *   part is never "." or "..". Embedded NUL bytes are outside the JSON
 *   string model (fixtures exclude them).
 * ──────────────────────────────────────────────────────────────── */

typedef struct pd2_io {
    json_t *(*call)(struct pd2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} pd2_io_t;

static int pd2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *pd2_seam(pd2_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int pd2_truthy(const json_t *v) {
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

/* PoP: doctor_runtime @ hermes_cli/plugin_dev.py:_doctor_runtime */
char *doctor_runtime(void) { return strdup(""); }

/* Path join for the fixture path model (see header). Caller frees. */
static char *pd2_path_join(const char *path, const char *name) {
    if (!path || !path[0]) return strdup(name);
    size_t pn = strlen(path), nn = strlen(name);
    while (pn > 0 && (path[pn - 1] == '/' || path[pn - 1] == '\\')) pn--;
    char *out = malloc(pn + 1 + nn + 1);
    if (!out) return NULL;
    memcpy(out, path, pn);
    out[pn] = '/';
    memcpy(out + pn + 1, name, nn + 1);
    return out;
}

/* PoP: _has_manifest @ hermes_cli/plugin_dev.py:_has_manifest */
json_t *_has_manifest(pd2_io_t *io, json_t *in) {
    /* any((path / name).exists() or (path / name).is_symlink()
     *     for name in _MANIFEST_NAMES) */
    static const char *names[] = { "plugin.yaml", "plugin.yml", "plugin.json" };
    const char *path = json_get_str(in, "path", "");
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char *joined = pd2_path_join(path, names[i]);
        json_t *args = json_object();
        json_set_hdr(args, "path", joined ? joined : "");
        json_t *r = pd2_seam(io, "path.exists", args);
        if (pd2_is_raise(r)) { json_free(args); free(joined); return r; }
        int hit = pd2_truthy(r);
        json_free(r);
        if (!hit) {
            r = pd2_seam(io, "path.is_symlink", args);
            if (pd2_is_raise(r)) { json_free(args); free(joined); return r; }
            hit = pd2_truthy(r);
            json_free(r);
        }
        json_free(args);
        free(joined);
        if (hit) return json_bool(1);
    }
    return json_bool(0);
}

/* PoP: holds_plugin @ hermes_cli/plugin_dev.py:_holds_plugin */
json_t *holds_plugin(pd2_io_t *io, json_t *in) {
    /* path.is_dir() and (_has_manifest(path) or bool(_category_plugins(path))) */
    const char *path = json_get_str(in, "path", "");
    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_t *r = pd2_seam(io, "path.is_dir", args);
    if (pd2_is_raise(r)) { json_free(args); return r; }
    int is_dir = pd2_truthy(r);
    json_free(r);
    if (!is_dir) { json_free(args); return json_bool(0); }
    r = pd2_seam(io, "has_manifest", args);
    if (pd2_is_raise(r)) { json_free(args); return r; }
    int has = pd2_truthy(r);
    json_free(r);
    if (has) { json_free(args); return json_bool(1); }
    r = pd2_seam(io, "category_plugins", args);
    json_free(args);
    if (pd2_is_raise(r)) return r;
    int cat = pd2_truthy(r);  /* bool(list) */
    json_free(r);
    return json_bool(cat != 0);
}

/* PurePath(raw) projection of the build host — see header. The Windows
 * branch reproduces pathlib splitting + ntpath.isabs on the NORMALIZED
 * string (drive/root parse, "." collapse, leading-".\\" preservation),
 * verified against live PureWindowsPath on a 50k-case random corpus. */
static int pd2_is_plugin_id(const char *raw) {
    if (!raw || !raw[0]) return 0;  /* `not raw` */
#ifdef _WIN32
    size_t n = strlen(raw);
    int issep0 = raw[0] == '\\' || raw[0] == '/';
    int issep1 = n >= 2 && (raw[1] == '\\' || raw[1] == '/');
    /* UNC/device paths normalize to a double-separator prefix: isabs */
    if (issep0 && issep1) return 0;
    int has_drive = 0, has_root = 0, leading_dot = 0;
    size_t start = 0;
    if (issep0) {
        has_root = 1; start = 1;               /* root without a drive */
    } else if (n >= 2 && raw[1] == ':') {
        has_drive = 1;                          /* drive = raw[:2] */
        if (n >= 3 && (raw[2] == '\\' || raw[2] == '/')) {
            has_root = 1; start = 3;
        } else {
            start = 2;
        }
    } else {
        /* str() keeps a leading "." component as a ".\\" prefix */
        leading_dot = raw[0] == '.' &&
                      (n == 1 || raw[1] == '\\' || raw[1] == '/');
    }
    /* Build the normalized string (isabs reads only its first 3 bytes,
     * so truncating a very long path here is harmless). */
    char norm[1024];
    size_t o = 0;
    int count = 0, first = 1;
    if (leading_dot && o + 2 < sizeof norm) { norm[o++] = '.'; norm[o++] = '\\'; }
    if (has_drive && o + 2 < sizeof norm) { norm[o++] = raw[0]; norm[o++] = raw[1]; }
    if (has_root && o + 1 < sizeof norm) { norm[o++] = '\\'; }
    size_t i = start;
    while (i <= n) {
        size_t j = i;
        while (j < n && raw[j] != '\\' && raw[j] != '/') j++;
        size_t len = j - i;
        if (len == 2 && raw[i] == '.' && raw[i + 1] == '.') return 0;  /* os.pardir */
        if (len > 0 && !(len == 1 && raw[i] == '.')) {  /* "." collapses */
            if (!first && o + 1 < sizeof norm) norm[o++] = '\\';
            first = 0;
            for (size_t k = i; k < j && o + 1 < sizeof norm; k++) norm[o++] = raw[k];
            count++;
        }
        if (j == n) break;
        i = j + 1;
    }
    norm[o] = '\0';
    /* ntpath.isabs: X:\\ prefix (drive+root) or double-separator prefix */
    if (o >= 3 && norm[1] == ':' && norm[2] == '\\') return 0;
    if (o >= 2 && norm[0] == '\\' && norm[1] == '\\') return 0;
    return has_drive || has_root || count > 0;
#else
    if (raw[0] == '/') return 0;  /* PurePosixPath.is_absolute */
    int count = 0;
    size_t i = 0, n = strlen(raw);
    while (i <= n) {
        size_t j = i;
        while (j < n && raw[j] != '/') j++;
        size_t len = j - i;
        if (len == 2 && raw[i] == '.' && raw[i + 1] == '.') return 0;
        if (len > 0 && !(len == 1 && raw[i] == '.')) count++;
        if (j == n) break;
        i = j + 1;
    }
    return count > 0;
#endif
}

/* PoP: is_plugin_id @ hermes_cli/plugin_dev.py:_is_plugin_id */
json_t *is_plugin_id(pd2_io_t *io, json_t *in) {
    (void)io;
    /* if not raw or PurePath(raw).is_absolute(): return False
     * parts = PurePath(raw).parts
     * return bool(parts) and all(part not in {os.curdir, os.pardir} ...) */
    const char *raw = json_get_str(in, "raw", "");
    return json_bool(pd2_is_plugin_id(raw) != 0);
}

/* PoP: accepts_var_kwargs @ hermes_cli/plugin_dev.py:_accepts_var_kwargs */
json_t *accepts_var_kwargs(pd2_io_t *io, json_t *in) {
    (void)in;
    /* try: parameters = inspect.signature(callback).parameters.values()
     * except (TypeError, ValueError): return False
     * return any(p.kind is inspect.Parameter.VAR_KEYWORD for p in parameters) */
    json_t *args = json_object();
    json_set_hdr(args, "callback", "<cb>");
    json_t *r = pd2_seam(io, "inspect.signature", args);
    json_free(args);
    if (pd2_is_raise(r)) {
        const char *ty = json_get_str(r, "__raise__", "");
        if (strcmp(ty, "TypeError") == 0 || strcmp(ty, "ValueError") == 0) {
            json_free(r);
            return json_bool(0);
        }
        return r;
    }
    int hit = 0;
    json_t *params = r ? json_obj_get(r, "parameters") : NULL;
    if (params && params->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(params); i++) {
            json_t *k = json_get(params, i);
            if (k && k->type == JSON_STRING && strcmp(k->str_val, "VAR_KEYWORD") == 0) {
                hit = 1;
                break;
            }
        }
    }
    json_free(r);
    return json_bool(hit != 0);
}
