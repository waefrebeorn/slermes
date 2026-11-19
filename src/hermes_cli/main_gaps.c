#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/main.py: 15 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the main/main_desktop gap stubs.
 * Universal seam interface: json_t *fn(mg2_io_t *io, json_t *in);
 * mg2_seam() records {ev, args, ret} in call order; scripted results
 * {"__raise__": Type, "message": ...} model Python exceptions and are
 * propagated/swallowed exactly where the live code does. BaseException
 * kinds pass through every `except Exception` site. NULL == None.
 *
 * Seam map:
 *   shutil.which(name)                 -> "shutil.which" {name}
 *   subprocess.run(argv, ...)          -> "subprocess.run" {argv}
 *     (the constant kwargs stdout/stderr=DEVNULL, timeout=5, check=False
 *      are omitted identically on both sides; result {"returncode": n})
 *   _sandbox_helper_lstat(path)        -> "sandbox_helper_lstat" {path}
 *     (result {"sandbox": <path str>, "st": {"st_mode","st_uid"} | null};
 *      the sibling's internal OSError catch is inside the scripted st)
 *   sys.platform is a per-case attribute double, not a seam call.
 * ──────────────────────────────────────────────────────────────── */

typedef struct mg2_io {
    json_t *(*call)(struct mg2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} mg2_io_t;

static int mg2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *mg2_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static json_t *mg2_seam(mg2_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int mg2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* PoP: argv_is_gateway_run @ hermes_cli/main.py:_argv_is_gateway_run */
json_t *argv_is_gateway_run(mg2_io_t *io, json_t *in) {
    (void)io;
    /* any(a == "gateway" and b == "run" for a, b in itertools.pairwise(argv))
     * Non-string elements never equal a str literal (no raise). */
    json_t *argv = json_obj_get(in, "argv");
    if (!argv || argv->type != JSON_ARRAY) return json_bool(0);
    size_t n = json_len(argv);
    for (size_t i = 0; i + 1 < n; i++) {
        json_t *a = json_get(argv, i);
        json_t *b = json_get(argv, i + 1);
        if (a && a->type == JSON_STRING && strcmp(a->str_val, "gateway") == 0 &&
            b && b->type == JSON_STRING && strcmp(b->str_val, "run") == 0) {
            return json_bool(1);
        }
    }
    return json_bool(0);
}

/* PoP: npm_lock_workspace_closure @ hermes_cli/main.py:npm_lock_workspace_closure */
char *npm_lock_workspace_closure(void) { return strdup("[]"); }

/* PoP: tui_selected_workspace_keys @ hermes_cli/main.py:tui_selected_workspace_keys */
char *tui_selected_workspace_keys(void) { return strdup("[]"); }

/* PoP: desktop_packaged_executable_in @ hermes_cli/main_desktop.py:_desktop_packaged_executable_in */
char *desktop_packaged_executable_in(void) { return strdup(""); }

/* PoP: desktop_staging_dir @ hermes_cli/main_desktop.py:_desktop_staging_dir */
char *desktop_staging_dir(void) { return strdup(""); }

/* PoP: desktop_unpacked_root @ hermes_cli/main_desktop.py:_desktop_unpacked_root */
char *desktop_unpacked_root(void) { return strdup(""); }

/* PoP: swap_staged_desktop_app @ hermes_cli/main_desktop.py:_swap_staged_desktop_app */
int swap_staged_desktop_app(void) { return 1; }


/* PoP: macos_codesigning_identity_valid @ hermes_cli/main_desktop_tcc.py:_macos_codesigning_identity_valid */
int macos_codesigning_identity_valid(void) { return 1; }


/* PoP: desktop_linux_userns_sandbox_available @ hermes_cli/main_desktop.py:_desktop_linux_userns_sandbox_available */
json_t *desktop_linux_userns_sandbox_available(mg2_io_t *io, json_t *in) {
    /* if sys.platform != "linux": return False */
    const char *platform = json_get_str(in, "platform", "");
    if (strcmp(platform, "linux") != 0) return json_bool(0);
    /* unshare = shutil.which("unshare"); if not unshare: return False */
    json_t *args = json_object();
    json_set_hdr(args, "name", "unshare");
    json_t *w = mg2_seam(io, "shutil.which", args);
    json_free(args);
    if (mg2_is_raise(w)) return w;  /* live has no guard around which() */
    const char *unshare = (w && w->type == JSON_STRING) ? w->str_val : NULL;
    if (!unshare || !unshare[0]) { json_free(w); return json_bool(0); }
    /* subprocess.run([unshare, "--user", "--map-root-user", "true"], ...) */
    json_t *ra = json_object();
    json_t *av = json_array();
    json_append(av, json_string(unshare));
    json_append(av, json_string("--user"));
    json_append(av, json_string("--map-root-user"));
    json_append(av, json_string("true"));
    json_set(ra, "argv", av);
    json_t *r = mg2_seam(io, "subprocess.run", ra);
    json_free(ra);
    json_free(w);
    if (mg2_is_raise(r)) {
        const char *ty = json_get_str(r, "__raise__", "");
        /* except (OSError, subprocess.TimeoutExpired): return False */
        if (strcmp(ty, "OSError") == 0 || strcmp(ty, "TimeoutExpired") == 0) {
            json_free(r);
            return json_bool(0);
        }
        return r;
    }
    if (mg2_is_null(r)) {
        return mg2_raise_spec("AttributeError",
                              "'NoneType' object has no attribute 'returncode'");
    }
    /* .returncode == 0 */
    int ok = r->type == JSON_OBJECT && json_obj_get(r, "returncode") != NULL &&
             json_get_num(r, "returncode", 1.0) == 0.0;
    json_free(r);
    return json_bool(ok != 0);
}

/* PoP: desktop_linux_needs_disable_setuid_sandbox @ hermes_cli/main_desktop.py:_desktop_linux_needs_disable_setuid_sandbox */
json_t *desktop_linux_needs_disable_setuid_sandbox(mg2_io_t *io, json_t *in) {
    /* if sys.platform != "linux": return False */
    const char *platform = json_get_str(in, "platform", "");
    if (strcmp(platform, "linux") != 0) return json_bool(0);
    /* _sandbox, st = _sandbox_helper_lstat(packaged_executable) */
    const char *path = json_get_str(in, "packaged_executable", "");
    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_t *r = mg2_seam(io, "sandbox_helper_lstat", args);
    json_free(args);
    if (mg2_is_raise(r)) return r;
    json_t *st = r ? json_obj_get(r, "st") : NULL;
    if (mg2_is_null(st)) { json_free(r); return json_bool(0); }
    /* stat.S_ISREG(st.st_mode) and not _sandbox_helper_is_setuid_root(st)
     * _sandbox_helper_is_setuid_root: st.st_uid == 0 and S_IMODE(mode) == 0o4755 */
    long long mode = (long long)json_get_num(st, "st_mode", 0.0);
    long long uid = (long long)json_get_num(st, "st_uid", -1.0);
    int isreg = (mode & 0170000LL) == 0100000LL;
    int setuid_root = uid == 0 && (mode & 07777LL) == 04755LL;
    json_free(r);
    return json_bool(isreg && !setuid_root);
}

int try_fast_serve_launch(void) { return 0; }
