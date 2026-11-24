/*
 * cua_backend_daemon.c -- real C11 port of upstream Hermes
 *   tools/computer_use/cua_backend_daemon.py (decision half).
 *
 * The daemon's start/stop/proxy plumbing spawns and probes a real process
 * (``subprocess.Popen`` / ``spawn_owned`` / a socket status probe) and is not
 * ported.  Everything that DECIDES -- the app-bundle resolution, the fail-closed
 * codesign verdict, the launch vector, and the constructor's permission/manifest
 * policy -- is ported exactly, error text included.
 *
 * PoP: w19_cuabackend_0e9d63_resolve_app_path @ tools/computer_use/cua_backend_daemon.py:_resolve_cua_driver_app_path
 * PoP: w19_cuabackend_0e9d63_validate_signature @ tools/computer_use/cua_backend_daemon.py:_validate_cua_driver_app_signature
 * PoP: w19_cuabackend_0e9d63_spawn_command @ tools/computer_use/cua_backend_daemon.py:_embedded_daemon_spawn_command
 * PoP: w19_cuabackend_0e9d63_daemon_init @ tools/computer_use/cua_backend_daemon.py:_EmbeddedCuaDaemon.__init__
 */
#include "hermes_json.h"
#include "noc_w19_cuabackend_0e9d63.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact matches only: a suffixed identifier or other team is an impostor. */
static const char *const DRIVER_BUNDLE_ID = "com.trycua.driver";
static const char *const TEAM_IDS[] = { "4YEC26S9KF", "YCK386LBJ7", NULL };
/* Python repr of _CUA_DRIVER_TEAM_IDS. */
static const char *const TEAM_IDS_REPR = "('4YEC26S9KF', 'YCK386LBJ7')";
static const char *const APP_MARKER = ".app/Contents/MacOS/";
static const char *const EXE_SUFFIX = "Contents/MacOS/cua-driver";

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int is_ascii_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *trimmed(const char *s, size_t n) {
    char *p;
    while (n > 0 && is_ascii_space((unsigned char)*s)) { s++; n--; }
    while (n > 0 && is_ascii_space((unsigned char)s[n - 1])) n--;
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* A malloc'd buffer from NUL-terminated pieces. */
typedef struct {
    char *buf;
    size_t len, cap;
} obuf_t;

static void ob_put(obuf_t *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        char *nb;
        while (nc < b->len + n + 1) nc *= 2;
        nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb;
        b->cap = nc;
    }
    if (n) memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void ob_cstr(obuf_t *b, const char *s) { ob_put(b, s, strlen(s)); }

/* -- Python str repr for the error texts ------------------------------------- */

static char *py_repr(const char *s) {
    obuf_t b;
    int has_single = 0, has_double = 0;
    char quote = '\'';
    const char *p;
    memset(&b, 0, sizeof b);
    if (!s) s = "";
    for (p = s; *p; p++) {
        if (*p == '\'') has_single = 1;
        if (*p == '"') has_double = 1;
    }
    if (has_single && !has_double) quote = '"';
    ob_put(&b, &quote, 1);
    for (p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        char esc[16];
        if (c == (unsigned char)quote || c == '\\') { ob_put(&b, "\\", 1); ob_put(&b, (const char *)&c, 1); }
        else if (c == '\n') ob_cstr(&b, "\\n");
        else if (c == '\r') ob_cstr(&b, "\\r");
        else if (c == '\t') ob_cstr(&b, "\\t");
        else if (c < 0x20 || c == 0x7F) { snprintf(esc, sizeof esc, "\\x%02x", c); ob_cstr(&b, esc); }
        else ob_put(&b, (const char *)&c, 1);
    }
    ob_put(&b, &quote, 1);
    return b.buf ? b.buf : dupstr("''");
}

/* -- app bundle resolution --------------------------------------------------- */

char *w19_cuabackend_0e9d63_resolve_app_path(const char *realpath, int isfile, int x_ok) {
    const char *marker;
    size_t head_len;
    char *app, *exe;
    int ok;
    if (!realpath) return NULL;
    marker = strstr(realpath, APP_MARKER);
    if (!marker) return NULL;
    head_len = (size_t)(marker - realpath);
    app = (char *)malloc(head_len + 5);
    exe = (char *)malloc(head_len + 5 + 1 + strlen(EXE_SUFFIX) + 1);
    if (!app || !exe) { free(app); free(exe); return NULL; }
    memcpy(app, realpath, head_len);
    memcpy(app + head_len, ".app", 5);            /* head + ".app" */
    memcpy(exe, app, head_len + 4);
    exe[head_len + 4] = '/';
    memcpy(exe + head_len + 5, EXE_SUFFIX, strlen(EXE_SUFFIX) + 1);
    ok = isfile && x_ok;
    free(exe);
    if (!ok) { free(app); return NULL; }
    return app;
}

/* -- codesign verdict -------------------------------------------------------- */

typedef struct {
    char *key;
    char *val;
} fld_t;

static void fields_put(fld_t *flds, size_t *n, size_t cap, char *key, char *val) {
    size_t i;
    for (i = 0; i < *n; i++) {
        if (!strcmp(flds[i].key, key)) {          /* the FIRST occurrence in the text wins */
            free(key);
            free(val);
            return;
        }
    }
    if (*n < cap) {
        flds[*n].key = key;
        flds[*n].val = val;
        (*n)++;
    } else {
        free(key);
        free(val);
    }
}

static const char *fields_get(fld_t *flds, size_t n, const char *key, const char *def) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(flds[i].key, key)) return flds[i].val;
    return def;
}

static json_t *verdict(int ok, char *err) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    json_set(o, "error", err ? json_string(err) : json_null());
    free(err);
    return o;
}

json_t *w19_cuabackend_0e9d63_validate_signature(const char *app_path, int have_codesign,
                                                 int returncode, const char *stderr_text,
                                                 int allow_unsigned) {
    fld_t flds[32];
    size_t nflds = 0, i;
    char *identifier, *team, *ident_repr, *team_repr, *err;
    obuf_t msg;
    int team_ok = 0, k;

    for (i = 0; i < sizeof flds / sizeof flds[0]; i++) { flds[i].key = NULL; flds[i].val = NULL; }

    if (!have_codesign)
        return verdict(0, dupstr("codesign is required to verify CuaDriver.app before launching it."));

    if (returncode != 0) {
        char *bare = trimmed(stderr_text ? stderr_text : "", strlen(stderr_text ? stderr_text : ""));
        memset(&msg, 0, sizeof msg);
        ob_cstr(&msg, "CuaDriver.app at ");
        ob_cstr(&msg, app_path ? app_path : "");
        ob_cstr(&msg, " is not code-signed; refusing to launch it (");
        ob_cstr(&msg, bare ? bare : "");
        ob_cstr(&msg, ")");
        free(bare);
        return verdict(0, msg.buf ? msg.buf : dupstr("not code-signed"));
    }

    /* parts = [line.partition("=") for line in (proc.stderr or "").splitlines()]
     * fields = {k.strip(): v.strip() for k, sep, v in reversed(parts) if sep}
     * -> the FIRST occurrence of a key wins, so keep the first one seen. */
    {
        const char *q = stderr_text ? stderr_text : "";
        const char *qe = q + strlen(q);
        size_t idx = 0;
        while (q < qe && idx < 256) {
            const char *nl = q;
            const char *eq = NULL, *t;
            while (nl < qe && *nl != '\n' && *nl != '\r') nl++;
            for (t = q; t < nl; t++) if (*t == '=') { eq = t; break; }
            if (eq) {
                char *key = trimmed(q, (size_t)(eq - q));
                char *val = trimmed(eq + 1, (size_t)(nl - eq - 1));
                if (key && val) fields_put(flds, &nflds, sizeof flds / sizeof flds[0], key, val);
                else { free(key); free(val); }
            }
            if (nl < qe && *nl == '\r') { nl++; if (nl < qe && *nl == '\n') nl++; }
            else if (nl < qe) nl++;
            q = nl;
            idx++;
        }
    }

    identifier = dupstr(fields_get(flds, nflds, "Identifier", ""));
    team = dupstr(fields_get(flds, nflds, "TeamIdentifier", ""));
    ident_repr = py_repr(identifier);
    team_repr = py_repr(team);

    if (!identifier || strcmp(identifier, DRIVER_BUNDLE_ID) != 0) {
        memset(&msg, 0, sizeof msg);
        ob_cstr(&msg, "CuaDriver.app at ");
        ob_cstr(&msg, app_path ? app_path : "");
        ob_cstr(&msg, " has identifier ");
        ob_cstr(&msg, ident_repr ? ident_repr : "''");
        ob_cstr(&msg, ", expected ");
        {
            char *want = py_repr(DRIVER_BUNDLE_ID);
            ob_cstr(&msg, want ? want : "''");
            free(want);
        }
        ob_cstr(&msg, "; refusing to launch it.");
        err = msg.buf ? msg.buf : dupstr("identifier mismatch");
        goto done;
    }

    for (k = 0; TEAM_IDS[k]; k++)
        if (!strcmp(team, TEAM_IDS[k])) { team_ok = 1; break; }
    if (!team_ok) {
        int unsigned_allowed = (!team[0] || !strcmp(team, "not set")) && allow_unsigned;
        if (unsigned_allowed) {
            err = NULL;
            goto done;
        }
        memset(&msg, 0, sizeof msg);
        ob_cstr(&msg, "CuaDriver.app at ");
        ob_cstr(&msg, app_path ? app_path : "");
        ob_cstr(&msg, " is signed by team ");
        ob_cstr(&msg, team_repr ? team_repr : "''");
        ob_cstr(&msg, ", expected one of ");
        ob_cstr(&msg, TEAM_IDS_REPR);
        ob_cstr(&msg, "; refusing to launch it. (Set computer_use.allow_unsigned_driver: true in "
                     "config.yaml only for local unsigned driver builds.)");
        err = msg.buf ? msg.buf : dupstr("team mismatch");
        goto done;
    }
    err = NULL;

done:
    for (i = 0; i < nflds; i++) { free(flds[i].key); free(flds[i].val); }
    free(identifier);
    free(team);
    free(ident_repr);
    free(team_repr);
    {
        json_t *out = verdict(err ? 0 : 1, err);
        return out;
    }
}

/* -- launch vector ----------------------------------------------------------- */

static json_t *spawn_result(int ok, json_t *command, char *err) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    json_set(o, "command", command ? command : json_null());
    json_set(o, "error", err ? json_string(err) : json_null());
    free(err);
    return o;
}

json_t *w19_cuabackend_0e9d63_spawn_command(const char *driver_cmd, const json_t *serve_args,
                                            const char *platform, const char *app_path,
                                            const char *realpath, int isfile, int x_ok,
                                            int have_codesign, int returncode,
                                            const char *stderr_text, int allow_unsigned) {
    json_t *cmd = json_array();
    size_t i, nargs = (serve_args && serve_args->type == JSON_ARRAY) ? json_len(serve_args) : 0;

    if (!platform || strcmp(platform, "darwin") != 0) {
        json_append(cmd, json_string(driver_cmd ? driver_cmd : ""));
        for (i = 0; i < nargs; i++) {
            json_t *e = json_get(serve_args, i);
            json_append(cmd, json_copy(e));
        }
        return spawn_result(1, cmd, NULL);
    }

    {
        char *resolved = (app_path && *app_path) ? dupstr(app_path)
                                                 : w19_cuabackend_0e9d63_resolve_app_path(realpath, isfile, x_ok);
        if (!resolved) {
            json_free(cmd);
            return spawn_result(0, NULL, dupstr("CuaDriver.app is required for private computer-use sessions "
                                                "on macOS. Run `hermes computer-use install` to restore it."));
        }
        {
            json_t *sig = w19_cuabackend_0e9d63_validate_signature(resolved, have_codesign, returncode,
                                                                  stderr_text, allow_unsigned);
            json_t *ok = json_obj_get(sig, "ok");
            if (!ok || !ok->bool_val) {
                json_t *e = json_obj_get(sig, "error");
                char *err = dupstr((e && e->type == JSON_STRING) ? e->str_val : "");
                json_free(sig);
                free(resolved);
                json_free(cmd);
                return spawn_result(0, NULL, err);
            }
            json_free(sig);
        }
        json_append(cmd, json_string("/usr/bin/open"));
        json_append(cmd, json_string("-n"));
        json_append(cmd, json_string("-g"));
        json_append(cmd, json_string("-a"));
        json_append(cmd, json_string(resolved));
        json_append(cmd, json_string("--args"));
        for (i = 0; i < nargs; i++) {
            json_t *e = json_get(serve_args, i);
            json_append(cmd, json_copy(e));
        }
        free(resolved);
    }
    return spawn_result(1, cmd, NULL);
}

/* -- daemon constructor policy ----------------------------------------------- */

static json_t *init_fail(char *err) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", err ? json_string(err) : json_null());
    json_set(o, "permission_mode", json_null());
    json_set(o, "capability_manifest", json_null());
    json_set(o, "manifest_applies", json_bool(false));
    json_set(o, "socket_path", json_null());
    free(err);
    return o;
}

json_t *w19_cuabackend_0e9d63_daemon_init(const char *driver_cmd, const char *permission_mode,
                                          const char *capability_manifest, const char *manifest_abspath,
                                          int manifest_exists, int mode_independent,
                                          const char *platform, const char *token, const char *tmpdir) {
    char *manifest, *abs_manifest = NULL;
    json_t *out;
    obuf_t path;

    if (!permission_mode ||
        (strcmp(permission_mode, "unrestricted") != 0 && strcmp(permission_mode, "bounded") != 0))
        return init_fail(dupstr("embedded permission override supports unrestricted or bounded only"));

    manifest = trimmed(capability_manifest ? capability_manifest : "",
                       strlen(capability_manifest ? capability_manifest : ""));
    if (!manifest) return init_fail(dupstr("out of memory"));
    if (!manifest[0] && !strcmp(permission_mode, "bounded")) {
        free(manifest);
        return init_fail(dupstr("bounded permission mode requires computer_use.capability_manifest"));
    }
    if (manifest[0]) {
        abs_manifest = dupstr(manifest_abspath ? manifest_abspath : manifest);
        if (!abs_manifest) { free(manifest); return init_fail(dupstr("out of memory")); }
        if (!manifest_exists) {
            memset(&path, 0, sizeof path);
            ob_cstr(&path, "capability manifest not found: ");
            ob_cstr(&path, abs_manifest);
            free(manifest);
            free(abs_manifest);
            return init_fail(path.buf ? path.buf : dupstr("capability manifest not found"));
        }
    }

    out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "error", json_null());
    json_set(out, "permission_mode", json_string(permission_mode));
    json_set(out, "capability_manifest",
             (abs_manifest && abs_manifest[0]) ? json_string(abs_manifest) : json_null());
    {
        int applies = abs_manifest && abs_manifest[0] &&
                      (!strcmp(permission_mode, "bounded") || mode_independent);
        json_set(out, "manifest_applies", json_bool(applies ? true : false));
    }
    memset(&path, 0, sizeof path);
    if (platform && !strcmp(platform, "win32")) {
        ob_cstr(&path, "\\\\.\\pipe\\hermes-cua-");
        ob_cstr(&path, token ? token : "");
    } else {
        /* os.path.join(tempfile.gettempdir(), f"hc-{token}.sock") (posix join) */
        if (tmpdir && *tmpdir) {
            ob_cstr(&path, tmpdir);
            if (path.len && path.buf[path.len - 1] != '/') ob_cstr(&path, "/");
        }
        ob_cstr(&path, "hc-");
        ob_cstr(&path, token ? token : "");
        ob_cstr(&path, ".sock");
    }
    json_set(out, "socket_path", json_string(path.buf ? path.buf : ""));
    free(path.buf);
    free(manifest);
    free(abs_manifest);
    (void)driver_cmd;
    return out;
}
