/* port_main_desktop_flow.c — FLOW-core port of hermes_cli/main_desktop.py
 * (desktop launch/lifecycle decision chains). See include/port_main_desktop_flow.h
 * for the seam table and the representation contract. libjson-only; no real
 * I/O inside the decision core.
 */
#include "port_main_desktop_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── seam dispatch ─────────────────────────────────────────────────────── */
/* CPython OSError(errno, msg) promotes the BASE OSError to the errno's
 * subclass (PermissionError, FileNotFoundError, ...); a subclass constructor
 * does NOT re-promote. The oracle materializes scripted raises with the real
 * OSError(errno, msg), so a scripted {"type":"OSError","errno":N} escapes as
 * the subclass. Mirror that here at materialization so both the escaping-ret
 * projection AND the type-specific catch sites (PermissionError retry, etc.)
 * see the promoted name. Only the base "OSError" with a numeric errno maps. */
static void promote_oserror(json_t *r) {
    if (!r || r->type != JSON_OBJECT) return;
    json_t *t = json_obj_get(r, "__raise__");
    if (!t || t->type != JSON_STRING || strcmp(t->str_val, "OSError") != 0) return;
    json_t *en = json_obj_get(r, "errno");
    if (!en || en->type != JSON_NUMBER) return;
    const char *sub = NULL;
    switch ((long)en->num_val) {
        case 1:  case 13: sub = "PermissionError";      break; /* EPERM/EACCES */
        case 2:           sub = "FileNotFoundError";    break; /* ENOENT */
        case 3:           sub = "ProcessLookupError";   break; /* ESRCH */
        case 4:           sub = "InterruptedError";     break; /* EINTR */
        case 10:          sub = "ChildProcessError";    break; /* ECHILD */
        case 11:          sub = "BlockingIOError";      break; /* EAGAIN */
        case 17:          sub = "FileExistsError";      break; /* EEXIST */
        case 20:          sub = "NotADirectoryError";   break; /* ENOTDIR */
        case 21:          sub = "IsADirectoryError";    break; /* EISDIR */
        case 32:          sub = "BrokenPipeError";      break; /* EPIPE */
        default: return;   /* EIO/EXDEV/... stay plain OSError */
    }
    json_set_hdr(r, "__raise__", sub);
}

json_t *mdf_call(mdf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    json_t *r = io->call(io, name, args);
    promote_oserror(r);
    return r;
}

bool mdf_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message ? message : "");
    return o;
}

/* sys.exit(code): code keeps its JSON type (int 0 vs str "0" matters live). */
static json_t *exit_node(double code) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", "SystemExit");
    json_set(o, "code", json_number(code));
    return o;
}

/* ── Python truthiness / type tests ────────────────────────────────────── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ── exception classification ──────────────────────────────────────────── */
static const char *exc_type(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool exc_is_systemexit(const json_t *r) {
    return strcmp(exc_type(r), "SystemExit") == 0;
}
static bool exc_is_oserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "OSError") == 0 || strcmp(t, "PermissionError") == 0
        || strcmp(t, "FileNotFoundError") == 0 || strcmp(t, "FileExistsError") == 0
        || strcmp(t, "BlockingIOError") == 0 || strcmp(t, "IsADirectoryError") == 0
        || strcmp(t, "NotADirectoryError") == 0 || strcmp(t, "InterruptedError") == 0
        || strcmp(t, "TimeoutError") == 0 || strcmp(t, "ProcessLookupError") == 0;
}
/* str(exc) parity: OSError(errno, msg) -> "[Errno N] msg"; else msg. */
static char *exc_str(const json_t *r) {
    const char *msg = json_get_str(r, "message", "");
    json_t *en = json_obj_get(r, "errno");
    size_t cap = strlen(msg) + 48;
    char *out = malloc(cap);
    if (en && en->type == JSON_NUMBER)
        snprintf(out, cap, "[Errno %.0f] %s", en->num_val, msg);
    else
        snprintf(out, cap, "%s", msg);
    return out;
}
/* Normalize an escaping raise descriptor to the oracle's proj_exc shape:
 * SystemExit -> {__raise__, code}; others -> {__raise__, message=str(exc)}. */
static json_t *propagate(json_t *r) {
    if (!mdf_raised(r)) return r;
    if (exc_is_systemexit(r)) {
        json_t *code = json_obj_get(r, "code");
        json_t *o = json_object();
        json_set_hdr(o, "__raise__", "SystemExit");
        json_set(o, "code", code ? json_copy(code) : json_null());
        json_free(r);
        return o;
    }
    char *m = exc_str(r);
    json_t *o = raise_desc(exc_type(r), m);
    free(m);
    json_free(r);
    return o;
}

/* ── string helpers (ASCII fixture scope; see header) ──────────────────── */
static char *dupn(const char *s, size_t n) {
    char *o = malloc(n + 1);
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *ascii_upper(const char *s) {
    char *o = dupn(s ? s : "", strlen(s ? s : ""));
    for (char *p = o; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
    return o;
}
/* os.path.join for two POSIX components (oracle drives posixpath.join). */
static char *posix_join2(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return dupn(b, strlen(b));
    if (!a[0]) return dupn(b, strlen(b));
    size_t la = strlen(a), lb = strlen(b);
    bool slash = a[la - 1] == '/';
    char *o = malloc(la + lb + 2);
    memcpy(o, a, la);
    size_t k = la;
    if (!slash) o[k++] = '/';
    memcpy(o + k, b, lb);
    o[k + lb] = '\0';
    return o;
}
/* PurePosixPath(...).name (basename; "" for the root). */
static char *posix_basename(const char *s) {
    if (!s || !s[0]) return dupn("", 0);
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if (n == 1 && s[0] == '/') return dupn("", 0);
    size_t start = n;
    while (start > 0 && s[start - 1] != '/') start--;
    return dupn(s + start, n - start);
}
/* Membership in PurePosixPath(...).parts: compare whole '/'-separated
 * components (the root "/" component never equals a name fixture). */
static bool posix_parts_has(const char *s, const char *want) {
    if (!s || !want || !want[0]) return false;
    const char *p = s;
    while (*p) {
        while (*p == '/') p++;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - start);
        if (n == strlen(want) && memcmp(start, want, n) == 0) return true;
    }
    return false;
}
/* Python str repr() for the ASCII fixture scope: ' preferred, " when the
 * text contains ' but no "; backslash/quote/\n/\r/\t/control escapes. */
static char *py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    size_t k = 0;
    o[k++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            k += (size_t)snprintf(o + k, cap - k, "\\x%02x", c);
        } else o[k++] = (char)c;
    }
    o[k++] = q;
    o[k] = '\0';
    return o;
}
/* len(os.fsencode(s)) == UTF-8 byte length for valid-UTF-8 fixture texts. */
static size_t utf8_len(const char *s) { return s ? strlen(s) : 0; }

/* ── injected print / env helpers ──────────────────────────────────────── */
/* builtins.print(text) — effect seam (end="\n", file="stdout"). The oracle's
 * print double records text = sep.join(args) + end (the FULL written text,
 * trailing newline included) and appends that to the capture buffer, so the
 * seam's args.text must carry the end newline too. */
static json_t *do_print(mdf_io_t *io, const char *text) {
    const char *t = text ? text : "";
    size_t n = strlen(t);
    char *buf = malloc(n + 2);
    memcpy(buf, t, n);
    buf[n] = '\n';
    buf[n + 1] = '\0';
    json_t *a = json_object();
    json_set_hdr(a, "text", buf);
    free(buf);
    json_set_hdr(a, "end", "\n");
    json_set_hdr(a, "file", "stdout");
    json_t *r = mdf_call(io, "print", a);
    json_free(a);
    return r;
}
/* `key in os.environ` — state-resolved seam (in.os_environ). */
static bool env_contains(mdf_io_t *io, const char *key, bool *raised_out, json_t **raise_out) {
    json_t *a = json_object();
    json_set_hdr(a, "name", key);
    json_t *r = mdf_call(io, "env.contains", a);
    json_free(a);
    if (mdf_raised(r)) { *raised_out = true; *raise_out = r; return false; }
    bool has = jtruth(r);
    json_free(r);
    *raised_out = false;
    *raise_out = NULL;
    return has;
}
static json_t *field_str(const char *key, const char *val) {
    json_t *a = json_object();
    json_set_hdr(a, key, val ? val : "");
    return a;
}
/* getattr(args, name, default) truthiness on the in.args namespace object. */
static bool arg_truthy(json_t *in, const char *name) {
    json_t *args = json_obj_get(in, "args");
    return jtruth(json_obj_get(args, name));
}
static const char *platform_of(json_t *in) {
    return json_get_str(in, "platform", "linux");
}

/* ── _site_packages_install_kind ───────────────────────────────────────── */
/* PoP: mdf_site_packages_install_kind @ hermes_cli/main_desktop.py:_site_packages_install_kind */
json_t *mdf_site_packages_install_kind(mdf_io_t *io, json_t *in) {
    (void)io;
    const char *root = json_get_str(in, "project_root", "");
    if (posix_parts_has(root, "site-packages") || posix_parts_has(root, "dist-packages"))
        return json_string(posix_parts_has(root, "Cellar") ? "homebrew" : "pip");
    return json_null();
}

/* ── _explicit_profile_args ────────────────────────────────────────────── */
/* PoP: mdf_explicit_profile_args @ hermes_cli/main_desktop.py:_explicit_profile_args */
json_t *mdf_explicit_profile_args(mdf_io_t *io, json_t *in) {
    (void)in;
    json_t *p = mdf_call(io, "explicit_cli_profile", NULL);
    if (mdf_raised(p)) return propagate(p);
    json_t *out = json_array();
    if (jtruth(p)) {
        json_append(out, json_string("--profile"));
        json_append(out, json_copy(p));
    }
    json_free(p);
    return out;
}

/* ── _diagnose_esbuild_ignore_scripts ──────────────────────────────────── */
/* PoP: mdf_diagnose_esbuild_ignore_scripts @ hermes_cli/main_desktop.py:_diagnose_esbuild_ignore_scripts */
json_t *mdf_diagnose_esbuild_ignore_scripts(mdf_io_t *io, json_t *in) {
    json_t *output = json_obj_get(in, "output");
    const char *text = jtruth(output) ? jstr(output) : NULL;
    if (!text) text = "";
    bool esbuild = strstr(text, "@esbuild/") && strstr(text, "could not be found");
    bool ignore_scripts = strstr(text, "ignore-scripts") != NULL;
    if (!esbuild && !ignore_scripts) return json_null();
    static const char *LINES[5] = {
        "  \xe2\x9a\xa0 This looks like esbuild's native binary is missing \xe2\x80\x94 its @esbuild/<platform> package",
        "    was not installed, or `ignoreScripts: true` in your pnpm config skipped its postinstall.",
        "    Fix: run `pnpm config get ignore-scripts` \xe2\x80\x94 if true, remove it",
        "    (`pnpm config delete --global ignore-scripts`), then delete node_modules in the repo",
        "    root so the next build reinstalls every workspace.",
    };
    for (int i = 0; i < 5; i++) {
        json_t *r = do_print(io, LINES[i]);
        if (mdf_raised(r)) return propagate(r);
        json_free(r);
    }
    return json_null();
}

/* ── _macos_signing_downgrade_error ────────────────────────────────────── */
/* dict[key] with the live KeyError escape when the key is absent. */
static json_t *dict_req(json_t *obj, const char *key, json_t **out) {
    json_t *v = json_obj_get(obj, key);
    if (!v) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s'", key);
        return raise_desc("KeyError", msg);
    }
    *out = v;
    return NULL;
}
/* PoP: mdf_macos_signing_downgrade_error @ hermes_cli/main_desktop.py:_macos_signing_downgrade_error */
json_t *mdf_macos_signing_downgrade_error(mdf_io_t *io, json_t *in) {
    (void)io;
    json_t *installed = json_obj_get(in, "installed");
    json_t *rebuilt = json_obj_get(in, "rebuilt");
    json_t *team = NULL;
    json_t *err = dict_req(installed, "team", &team);
    if (err) return err;
    if (!jtruth(team)) return json_null();
    const char *team_s = jstr(team) ? jstr(team) : "";

    bool no_team = j_is_null(rebuilt);
    json_t *rteam = NULL;
    if (!no_team) {
        err = dict_req(rebuilt, "team", &rteam);
        if (err) return err;
        no_team = !jtruth(rteam);
    }
    if (no_team) {
        const char *fmt = "publisher-signed app (Team ID %s) would be replaced by a "
            "locally signed or unreadable build; kept the existing app. To update it, "
            "sign the rebuild with the publisher identity (CSC_LINK / "
            "APPLE_SIGNING_IDENTITY) and update again.";
        size_t cap = strlen(fmt) + strlen(team_s) + 1;
        char *msg = malloc(cap);
        snprintf(msg, cap, fmt, team_s);
        json_t *o = json_string(msg);
        free(msg);
        return o;
    }
    const char *rteam_s = jstr(rteam) ? jstr(rteam) : "";
    if (strcmp(team_s, rteam_s) != 0) {
        const char *fmt = "publisher Team ID %s does not match rebuilt %s; kept the "
            "existing app. Check the signing identity in desktop.macos_signing_identity "
            "and update again.";
        size_t cap = strlen(fmt) + strlen(team_s) + strlen(rteam_s) + 1;
        char *msg = malloc(cap);
        snprintf(msg, cap, fmt, team_s, rteam_s);
        json_t *o = json_string(msg);
        free(msg);
        return o;
    }
    json_t *iid = NULL, *rid = NULL;
    err = dict_req(installed, "identifier", &iid);
    if (err) return err;
    err = dict_req(rebuilt, "identifier", &rid);
    if (err) return err;
    if (jtruth(iid) && jtruth(rid) && strcmp(jstr(iid), jstr(rid)) != 0) {
        char *ri = py_repr_str(jstr(iid));
        char *rr = py_repr_str(jstr(rid));
        const char *fmt = "bundle identifier %s does not match rebuilt %s; kept the "
            "existing app. Align the build's bundle identifier and update again.";
        size_t cap = strlen(fmt) + strlen(ri) + strlen(rr) + 1;
        char *msg = malloc(cap);
        snprintf(msg, cap, fmt, ri, rr);
        json_t *o = json_string(msg);
        free(msg); free(ri); free(rr);
        return o;
    }
    json_t *verified = NULL;
    err = dict_req(rebuilt, "verified", &verified);
    if (err) return err;
    if (!jtruth(verified))
        return json_string("rebuilt bundle failed strict signature verification; kept the existing "
                           "app. Re-run the build; if it persists, inspect with `codesign -vvv`.");
    return json_null();
}

/* ── _sandbox_helper_is_setuid_root ────────────────────────────────────── */
/* PoP: mdf_sandbox_helper_is_setuid_root @ hermes_cli/main_desktop.py:_sandbox_helper_is_setuid_root */
json_t *mdf_sandbox_helper_is_setuid_root(mdf_io_t *io, json_t *in) {
    (void)io;
    json_t *st = json_obj_get(in, "st");
    double uid = json_get_num(st, "st_uid", -1);
    double mode = json_get_num(st, "st_mode", 0);
    /* stat.S_IMODE(mode) == mode & 0o7777 */
    bool ok = uid == 0.0 && (((long)mode) & 07777L) == 04755L;
    return json_bool(ok);
}

/* ── _windows_native_machine ───────────────────────────────────────────── */
/* PoP: mdf_windows_native_machine @ hermes_cli/main_desktop.py:_windows_native_machine */
json_t *mdf_windows_native_machine(mdf_io_t *io, json_t *in) {
    const char *plat = platform_of(in);
    if (strcmp(plat, "win32") == 0) {
        json_t *arch = mdf_call(io, "facts.native_arch", NULL);
        if (mdf_raised(arch)) return propagate(arch);
        /* dict.get(key, default) evaluates the default EAGERLY: platform.machine()
         * runs even when the arch key matches. */
        json_t *mach = mdf_call(io, "platform.machine", NULL);
        if (mdf_raised(mach)) { json_free(arch); return propagate(mach); }
        char *up = ascii_upper(jstr(mach) ? jstr(mach) : "");
        const char *a = jstr(arch) ? jstr(arch) : "";
        const char *mapped = NULL;
        if (strcmp(a, "arm64") == 0) mapped = "ARM64";
        else if (strcmp(a, "amd64") == 0) mapped = "AMD64";
        else if (strcmp(a, "x86") == 0) mapped = "X86";
        json_t *o = json_string(mapped ? mapped : up);
        free(up);
        json_free(arch);
        json_free(mach);
        return o;
    }
    json_t *mach = mdf_call(io, "platform.machine", NULL);
    if (mdf_raised(mach)) return propagate(mach);
    char *up = ascii_upper(jstr(mach) ? jstr(mach) : "");
    json_t *o = json_string(up);
    free(up);
    json_free(mach);
    return o;
}

/* ── _windows_user_runnable_pe_machines ────────────────────────────────── */
#define MDF_PE_I386  0x014C
#define MDF_PE_AMD64 0x8664
#define MDF_PE_ARM64 0xAA64
/* PoP: mdf_windows_user_runnable_pe_machines @ hermes_cli/main_desktop.py:_windows_user_runnable_pe_machines */
json_t *mdf_windows_user_runnable_pe_machines(mdf_io_t *io, json_t *in) {
    (void)in;
    json_t *k = mdf_call(io, "_kernel32", NULL);
    if (mdf_raised(k)) return propagate(k);
    json_free(k);
    static const int MACHINES[3] = { MDF_PE_ARM64, MDF_PE_AMD64, MDF_PE_I386 };
    int found[3];
    size_t n = 0;
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object();
        json_set(a, "machine", json_number(MACHINES[i]));
        json_t *r = mdf_call(io, "GetMachineTypeAttributes", a);
        json_free(a);
        if (mdf_raised(r)) return propagate(r);   /* no try here: escapes */
        double hresult = json_get_num(r, "hresult", 0);
        double attrs = json_get_num(r, "attributes", 0);
        json_free(r);
        if (hresult != 0) continue;               /* HRESULT nonzero = failure */
        if (((long)attrs) & 1L) found[n++] = MACHINES[i];
    }
    if (n == 0) return json_null();               /* runnable or None */
    for (size_t i = 1; i < n; i++) {              /* sorted ascending (set canon) */
        int v = found[i];
        size_t j = i;
        while (j > 0 && found[j - 1] > v) { found[j] = found[j - 1]; j--; }
        found[j] = v;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_number(found[i]));
    return out;
}

/* ── _expected_windows_pe_machines ─────────────────────────────────────── */
static json_t *pe_set(int a, int b, int c) {
    int v[3]; size_t n = 0;
    if (a >= 0) v[n++] = a;
    if (b >= 0) v[n++] = b;
    if (c >= 0) v[n++] = c;
    for (size_t i = 1; i < n; i++) {
        int x = v[i]; size_t j = i;
        while (j > 0 && v[j - 1] > x) { v[j] = v[j - 1]; j--; }
        v[j] = x;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_number(v[i]));
    return out;
}
/* PoP: mdf_expected_windows_pe_machines @ hermes_cli/main_desktop.py:_expected_windows_pe_machines */
json_t *mdf_expected_windows_pe_machines(mdf_io_t *io, json_t *in) {
    const char *plat = platform_of(in);
    if (strcmp(plat, "win32") == 0) {
        json_t *r = mdf_call(io, "_windows_user_runnable_pe_machines", NULL);
        if (mdf_raised(r)) {
            const char *t = exc_type(r);
            bool caught = exc_is_oserror(r)
                || strcmp(t, "AttributeError") == 0 || strcmp(t, "TypeError") == 0
                || strcmp(t, "ValueError") == 0;
            if (!caught) return propagate(r);
            json_free(r);
        } else if (jtruth(r)) {
            return r;                             /* non-empty set wins */
        } else {
            json_free(r);
        }
    }
    json_t *m = mdf_call(io, "_windows_native_machine", NULL);
    if (mdf_raised(m)) return propagate(m);
    char *machine = ascii_upper(jstr(m) ? jstr(m) : "");
    json_free(m);
    json_t *out;
    if (strcmp(machine, "AMD64") == 0 || strcmp(machine, "X86_64") == 0 || strcmp(machine, "X64") == 0)
        out = pe_set(MDF_PE_AMD64, MDF_PE_I386, -1);
    else if (strcmp(machine, "ARM64") == 0 || strcmp(machine, "AARCH64") == 0)
        out = pe_set(MDF_PE_ARM64, MDF_PE_AMD64, -1);
    else if (strcmp(machine, "X86") == 0 || strcmp(machine, "I386") == 0 || strcmp(machine, "I486") == 0
             || strcmp(machine, "I586") == 0 || strcmp(machine, "I686") == 0)
        out = pe_set(MDF_PE_I386, -1, -1);
    else
        out = pe_set(MDF_PE_AMD64, MDF_PE_ARM64, MDF_PE_I386);
    free(machine);
    return out;
}

/* ── _parse_pe_machine ─────────────────────────────────────────────────── */
static unsigned le16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static unsigned long le32(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8)
         | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
static int hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* file.read(n) seam: hex-encoded bytes -> malloc'd buffer. Returns a raise
 * descriptor on a scripted exception, else NULL (buffer handed out). */
static json_t *pe_read(mdf_io_t *io, const char *path, int n,
                       unsigned char **out, size_t *outlen) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set(a, "n", json_number(n));
    json_t *r = mdf_call(io, "file.read", a);
    json_free(a);
    if (mdf_raised(r)) return r;
    const char *hx = jstr(r);
    size_t hlen = hx ? strlen(hx) : 0;
    size_t blen = hlen / 2;
    unsigned char *buf = malloc(blen ? blen : 1);
    for (size_t i = 0; i < blen; i++) {
        int hi = hexval((unsigned char)hx[2 * i]);
        int lo = hexval((unsigned char)hx[2 * i + 1]);
        buf[i] = (unsigned char)(((hi < 0 ? 0 : hi) << 4) | (lo < 0 ? 0 : lo));
    }
    json_free(r);
    *out = buf;
    *outlen = blen;
    return NULL;
}
static json_t *pe_seek(mdf_io_t *io, const char *path, unsigned long long pos) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set(a, "pos", json_number((double)pos));
    json_t *r = mdf_call(io, "file.seek", a);
    json_free(a);
    return r;
}
/* PoP: mdf_parse_pe_machine @ hermes_cli/main_desktop.py:_parse_pe_machine */
json_t *mdf_parse_pe_machine(mdf_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    unsigned char *head = NULL, *pe_head = NULL, *sec = NULL;
    size_t head_len = 0, pe_len = 0, sec_len = 0;
    json_t *result = NULL;
    char msg[512];

    json_t *a = field_str("path", path);
    json_t *st = mdf_call(io, "path.stat", a);
    json_free(a);
    if (mdf_raised(st)) {
        if (exc_is_oserror(st)) {
            char *es = exc_str(st);
            snprintf(msg, sizeof msg, "unreadable: %s", es);
            free(es);
            result = raise_desc("ValueError", msg);
        } else {
            result = propagate(st);
        }
        goto done;
    }
    double file_size = json_get_num(st, "st_size", 0);
    json_free(st);
    if (file_size < 512) {
        snprintf(msg, sizeof msg,
                 "file is only %.0f bytes \xe2\x80\x94 far too small to be a Windows executable",
                 file_size);
        result = raise_desc("ValueError", msg);
        goto done;
    }
    a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "mode", "rb");
    json_t *op = mdf_call(io, "path.open", a);
    json_free(a);
    if (mdf_raised(op)) { result = propagate(op); goto done; }
    json_free(op);

    result = pe_read(io, path, 64, &head, &head_len);
    if (result) goto done;
    if (head_len < 64 || head[0] != 'M' || head[1] != 'Z') {
        result = raise_desc("ValueError",
            "missing MZ header \xe2\x80\x94 not a Windows executable (a truncated or non-binary file saved as .exe?)");
        goto done;
    }
    unsigned long long e_lfanew = le32(head + 0x3C);
    if (e_lfanew == 0 || (double)(e_lfanew + 24) > file_size) {
        result = raise_desc("ValueError",
            "corrupt DOS header: PE header offset points past end of file");
        goto done;
    }
    json_t *sk = pe_seek(io, path, e_lfanew);
    if (mdf_raised(sk)) { result = propagate(sk); goto done; }
    json_free(sk);
    result = pe_read(io, path, 24, &pe_head, &pe_len);
    if (result) goto done;
    if (pe_len < 24 || pe_head[0] != 'P' || pe_head[1] != 'E'
        || pe_head[2] != 0 || pe_head[3] != 0) {
        result = raise_desc("ValueError",
            "missing PE signature \xe2\x80\x94 corrupt executable header");
        goto done;
    }
    unsigned machine = le16(pe_head + 4);
    unsigned n_sections = le16(pe_head + 6);
    unsigned size_opt = le16(pe_head + 20);
    sk = pe_seek(io, path, e_lfanew + 24 + size_opt);
    if (mdf_raised(sk)) { result = propagate(sk); goto done; }
    json_free(sk);
    unsigned long long max_end = 0;
    for (unsigned i = 0; i < n_sections; i++) {
        free(sec); sec = NULL;
        result = pe_read(io, path, 40, &sec, &sec_len);
        if (result) goto done;
        if (sec_len < 40) {
            result = raise_desc("ValueError", "truncated PE section table");
            goto done;
        }
        unsigned long size_raw = le32(sec + 16);
        unsigned long ptr_raw = le32(sec + 20);
        unsigned long long end = (unsigned long long)ptr_raw + size_raw;
        if (end > max_end) max_end = end;
    }
    if ((double)max_end > file_size) {
        snprintf(msg, sizeof msg,
                 "truncated executable: file is %.0f bytes but its PE sections extend to %llu bytes",
                 file_size, max_end);
        result = raise_desc("ValueError", msg);
        goto done;
    }
    result = json_number(machine);
done:
    free(head);
    free(pe_head);
    free(sec);
    return result;
}

/* ── _packaged_node_pty_missing ────────────────────────────────────────── */
/* PoP: mdf_packaged_node_pty_missing @ hermes_cli/main_desktop.py:_packaged_node_pty_missing */
json_t *mdf_packaged_node_pty_missing(mdf_io_t *io, json_t *in) {
    const char *dist = json_get_str(in, "dist_dir", "");
    const char *plat = platform_of(in);
    char *j1 = posix_join2(dist, "node_modules");
    char *root = posix_join2(j1, "node-pty");
    free(j1);
    char *pj = posix_join2(root, "package.json");
    json_t *a = field_str("path", pj);
    json_t *isf = mdf_call(io, "path.is_file", a);
    json_free(a);
    free(pj);
    if (mdf_raised(isf)) { free(root); return propagate(isf); }
    bool has_pkg = jtruth(isf);
    json_free(isf);
    if (!has_pkg) { free(root); return json_bool(false); }

    j1 = posix_join2(root, "build");
    char *release = posix_join2(j1, "Release");
    free(j1);
    char *prebuilds = posix_join2(root, "prebuilds");
    free(root);
    size_t plen = strlen(plat);
    char *pattern = malloc(plen + 3);
    memcpy(pattern, plat, plen);
    memcpy(pattern + plen, "-*", 3);
    a = json_object();
    json_set_hdr(a, "path", prebuilds);
    json_set_hdr(a, "pattern", pattern);
    json_t *glob = mdf_call(io, "path.glob", a);
    json_free(a);
    free(pattern);
    free(prebuilds);
    if (mdf_raised(glob)) { free(release); return propagate(glob); }

    /* native_dirs = [build/Release, *prebuilds.glob(...)]; any() short-circuits
     * on the first dir whose rglob yields an item (a Path is always truthy).
     * A script queue hands back ONE path string per call; a bare string result
     * is a single match (the oracle's glob double coerces str -> [str]). */
    bool glob_str = j_is_str(glob);
    bool found = false;
    size_t n_dirs = 1 + (j_is_arr(glob) ? json_len(glob) : (glob_str ? 1 : 0));
    for (size_t i = 0; i < n_dirs && !found; i++) {
        const char *d = (i == 0) ? release
                        : (glob_str ? jstr(glob) : jstr(json_get(glob, i - 1)));
        if (!d) continue;
        a = field_str("path", d);
        json_t *isd = mdf_call(io, "path.is_dir", a);
        json_free(a);
        if (mdf_raised(isd)) { free(release); json_free(glob); return propagate(isd); }
        bool is_dir = jtruth(isd);
        json_free(isd);
        if (!is_dir) continue;
        a = json_object();
        json_set_hdr(a, "path", d);
        json_set_hdr(a, "pattern", "*.node");
        json_t *rg = mdf_call(io, "path.rglob", a);
        json_free(a);
        if (mdf_raised(rg)) { free(release); json_free(glob); return propagate(rg); }
        /* next(..., None) != None: a non-empty list OR a single path string. */
        found = (j_is_arr(rg) && json_len(rg) > 0) || j_is_str(rg);
        json_free(rg);
    }
    free(release);
    json_free(glob);
    return json_bool(!found);
}

/* ── _packaged_desktop_current_for_head ────────────────────────────────── */
/* PoP: mdf_packaged_desktop_current_for_head @ hermes_cli/main_desktop.py:_packaged_desktop_current_for_head */
json_t *mdf_packaged_desktop_current_for_head(mdf_io_t *io, json_t *in) {
    const char *dd = json_get_str(in, "desktop_dir", "");
    const char *pr = json_get_str(in, "project_root", "");
    json_t *a = field_str("desktop_dir", dd);
    json_t *res = mdf_call(io, "_packaged_resources_dir", a);
    json_free(a);
    if (mdf_raised(res)) return propagate(res);
    if (j_is_null(res)) { json_free(res); return json_bool(false); }
    a = field_str("resources", jstr(res) ? jstr(res) : "");
    json_free(res);
    json_t *stamp = mdf_call(io, "read_install_stamp", a);
    json_free(a);
    if (mdf_raised(stamp)) return propagate(stamp);
    if (!j_is_obj(stamp)) {
        const char *tn = "NoneType";
        if (stamp) {
            switch (stamp->type) {
                case JSON_ARRAY: tn = "list"; break;
                case JSON_STRING: tn = "str"; break;
                case JSON_NUMBER: tn = "int"; break;
                case JSON_BOOL: tn = "bool"; break;
                default: break;
            }
        }
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tn);
        json_free(stamp);
        return raise_desc("AttributeError", msg);
    }
    json_t *commit = json_obj_get(stamp, "commit");   /* plain dict.get */
    if (!j_is_str(commit)) { json_free(stamp); return json_bool(false); }
    a = json_object();
    json_set_hdr(a, "project_root", pr);
    json_t *ga = json_array();
    json_append(ga, json_string("rev-parse"));
    json_append(ga, json_string("HEAD"));
    json_set(a, "args", ga);
    json_t *git = mdf_call(io, "_run_git", a);
    json_free(a);
    if (mdf_raised(git)) { json_free(stamp); return propagate(git); }
    bool match = jstr(git) && strcmp(jstr(commit), jstr(git)) == 0;
    json_free(git);
    json_free(stamp);
    if (!match) return json_bool(false);
    a = json_object();
    json_set_hdr(a, "desktop_dir", dd);
    json_set_hdr(a, "project_root", pr);
    json_set(a, "source_mode", json_bool(false));
    json_t *bn = mdf_call(io, "_desktop_build_needed", a);
    json_free(a);
    if (mdf_raised(bn)) return propagate(bn);
    bool needed = jtruth(bn);
    json_free(bn);
    return json_bool(!needed);
}

/* ── _prefer_wsl_d3d12 ─────────────────────────────────────────────────── */
#define MDF_WSL_DXG "/dev/dxg"
static const char *MDF_WSL_DRIVERS[4] = {
    "/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so",
    "/usr/lib/aarch64-linux-gnu/dri/d3d12_dri.so",
    "/usr/lib64/dri/d3d12_dri.so",
    "/usr/lib/dri/d3d12_dri.so",
};
static const char *MDF_MESA_OVERRIDES[4] = {
    "GALLIUM_DRIVER", "MESA_LOADER_DRIVER_OVERRIDE",
    "LIBGL_ALWAYS_SOFTWARE", "LIBGL_DRIVERS_PATH",
};
/* Shared core: mutates env in place (borrowed). Returns a raise descriptor
 * or NULL (plain None return). */
static json_t *prefer_wsl_env(mdf_io_t *io, json_t *env) {
    for (int i = 0; i < 4; i++)
        if (json_has(env, MDF_MESA_OVERRIDES[i])) return NULL;
    json_t *wsl = mdf_call(io, "is_wsl", NULL);
    if (mdf_raised(wsl)) return propagate(wsl);
    bool is_wsl = jtruth(wsl);
    json_free(wsl);
    if (!is_wsl) return NULL;
    json_t *a = field_str("path", MDF_WSL_DXG);
    json_t *ex = mdf_call(io, "path.exists", a);
    json_free(a);
    if (mdf_raised(ex)) return propagate(ex);
    bool dxg = jtruth(ex);
    json_free(ex);
    if (!dxg) return NULL;
    for (int i = 0; i < 4; i++) {
        a = field_str("path", MDF_WSL_DRIVERS[i]);
        json_t *isf = mdf_call(io, "path.is_file", a);
        json_free(a);
        if (mdf_raised(isf)) return propagate(isf);
        bool yes = jtruth(isf);
        json_free(isf);
        if (yes) {
            json_set_hdr(env, "GALLIUM_DRIVER", "d3d12");
            return NULL;                          /* any() short-circuit */
        }
    }
    return NULL;
}
/* PoP: mdf_prefer_wsl_d3d12 @ hermes_cli/main_desktop.py:_prefer_wsl_d3d12 */
json_t *mdf_prefer_wsl_d3d12(mdf_io_t *io, json_t *in) {
    json_t *env = json_obj_get(in, "env");
    if (!env) return json_null();
    json_t *r = prefer_wsl_env(io, env);
    return r ? r : json_null();
}

/* ── _desktop_launch_env ───────────────────────────────────────────────── */
#define MDF_TMPDIR_MAX_BYTES 74   /* 107 - len("/scoped_dirXXXXXX/SingletonSocket") */
/* PoP: mdf_desktop_launch_env @ hermes_cli/main_desktop.py:_desktop_launch_env */
json_t *mdf_desktop_launch_env(mdf_io_t *io, json_t *in) {
    const char *plat = platform_of(in);
    json_t *env = mdf_call(io, "with_hermes_node_path", NULL);
    if (mdf_raised(env)) return propagate(env);
    if (!j_is_obj(env)) { json_free(env); env = json_object(); }
    json_t *result = NULL;

    const char *tmpdir = json_get_str(env, "TMPDIR", "");   /* plain dict.get */
    if (strcmp(plat, "linux") == 0 && utf8_len(tmpdir) > MDF_TMPDIR_MAX_BYTES) {
        json_set_hdr(env, "HERMES_DESKTOP_TMPDIR", tmpdir);
        json_t *sst = mdf_call(io, "socket_safe_tmpdir", NULL);
        if (mdf_raised(sst)) { result = propagate(sst); goto fail; }
        json_set(env, "TMPDIR", sst);
    }
    result = prefer_wsl_env(io, env);
    if (result) goto fail;

    if (arg_truthy(in, "fake_boot")) json_set_hdr(env, "HERMES_DESKTOP_BOOT_FAKE", "1");
    if (arg_truthy(in, "ignore_existing")) json_set_hdr(env, "HERMES_DESKTOP_IGNORE_EXISTING", "1");

    json_t *args = json_obj_get(in, "args");
    json_t *hr = json_obj_get(args, "hermes_root");
    if (jtruth(hr)) {
        json_t *a = field_str("path", jstr(hr) ? jstr(hr) : "");
        json_t *eu = mdf_call(io, "path.expanduser", a);
        json_free(a);
        if (mdf_raised(eu)) { result = propagate(eu); goto fail; }
        a = field_str("path", jstr(eu) ? jstr(eu) : "");
        json_free(eu);
        json_t *rs = mdf_call(io, "path.resolve", a);
        json_free(a);
        if (mdf_raised(rs)) { result = propagate(rs); goto fail; }
        json_set(env, "HERMES_DESKTOP_HERMES_ROOT", rs);
    }
    json_t *cwd = json_obj_get(args, "cwd");
    if (jtruth(cwd)) {
        json_t *a = field_str("path", jstr(cwd) ? jstr(cwd) : "");
        json_t *eu = mdf_call(io, "path.expanduser", a);
        json_free(a);
        if (mdf_raised(eu)) { result = propagate(eu); goto fail; }
        a = field_str("path", jstr(eu) ? jstr(eu) : "");
        json_free(eu);
        json_t *rs = mdf_call(io, "path.resolve", a);
        json_free(a);
        if (mdf_raised(rs)) { result = propagate(rs); goto fail; }
        json_set(env, "HERMES_DESKTOP_CWD", rs);
    } else {
        json_t *gc = mdf_call(io, "os.getcwd", NULL);
        if (mdf_raised(gc)) { result = propagate(gc); goto fail; }
        json_set(env, "HERMES_DESKTOP_CWD", gc);
    }

    json_t *opts = mdf_call(io, "_desktop_launch_options", NULL);
    if (mdf_raised(opts)) { result = propagate(opts); goto fail; }
    if (!j_is_arr(opts) || json_len(opts) < 5) {
        char umsg[96];
        snprintf(umsg, sizeof umsg,
                 "not enough values to unpack (expected 5, got %zu)",
                 j_is_arr(opts) ? json_len(opts) : (size_t)0);
        json_free(opts);
        result = raise_desc("ValueError", umsg);
        goto fail;
    }
    json_t *flags = json_copy(json_get(opts, 0));
    json_t *gpu_n = json_get(opts, 1);
    json_t *store_n = json_get(opts, 2);
    json_t *ozone_n = json_get(opts, 3);
    char *gpu = dupn(jstr(gpu_n) ? jstr(gpu_n) : "auto", strlen(jstr(gpu_n) ? jstr(gpu_n) : "auto"));
    char *store = dupn(jstr(store_n) ? jstr(store_n) : "auto", strlen(jstr(store_n) ? jstr(store_n) : "auto"));
    char *ozone = dupn(jstr(ozone_n) ? jstr(ozone_n) : "auto", strlen(jstr(ozone_n) ? jstr(ozone_n) : "auto"));
    bool a11y = jtruth(json_get(opts, 4));
    json_free(opts);

    bool raised = false;
    json_t *raise_r = NULL;
    if (strcmp(gpu, "auto") != 0) {
        bool has = env_contains(io, "HERMES_DESKTOP_DISABLE_GPU", &raised, &raise_r);
        if (raised) { result = propagate(raise_r); json_free(flags); goto fail2; }
        if (!has) json_set_hdr(env, "HERMES_DESKTOP_DISABLE_GPU", gpu);
    }
    if (strcmp(ozone, "auto") != 0) {
        bool has = env_contains(io, "ELECTRON_OZONE_PLATFORM_HINT", &raised, &raise_r);
        if (raised) { result = propagate(raise_r); json_free(flags); goto fail2; }
        if (!has) json_set_hdr(env, "ELECTRON_OZONE_PLATFORM_HINT", ozone);
    }
    if (!a11y) {
        bool has = env_contains(io, "HERMES_DESKTOP_RENDERER_ACCESSIBILITY", &raised, &raise_r);
        if (raised) { result = propagate(raise_r); json_free(flags); goto fail2; }
        if (!has) json_set_hdr(env, "HERMES_DESKTOP_RENDERER_ACCESSIBILITY", "0");
    }
    if (strcmp(plat, "linux") == 0) {
        bool has = env_contains(io, "HERMES_DESKTOP_PASSWORD_STORE", &raised, &raise_r);
        if (raised) { result = propagate(raise_r); json_free(flags); goto fail2; }
        if (!has) {
            const char *ps = NULL;
            json_t *ps_owned = NULL;
            if (strcmp(store, "auto") != 0) {
                ps = store;
            } else {
                ps_owned = mdf_call(io, "_detect_linux_password_store", NULL);
                if (mdf_raised(ps_owned)) {
                    result = propagate(ps_owned);
                    json_free(flags);
                    goto fail2;
                }
                ps = jstr(ps_owned);
            }
            if (ps && ps[0]) json_set_hdr(env, "HERMES_DESKTOP_PASSWORD_STORE", ps);
            json_free(ps_owned);
        }
    }
    free(gpu);
    free(store);
    free(ozone);
    result = json_array();
    json_append(result, env);                 /* adopts env */
    json_append(result, flags);               /* adopts flags */
    return result;
fail2:
    free(gpu);
    free(store);
    free(ozone);
fail:
    json_free(env);
    return result;
}

/* ── _check_desktop_skip_build ─────────────────────────────────────────── */
/* PoP: mdf_check_desktop_skip_build @ hermes_cli/main_desktop.py:_check_desktop_skip_build */
json_t *mdf_check_desktop_skip_build(mdf_io_t *io, json_t *in) {
    const char *dd = json_get_str(in, "desktop_dir", "");
    const char *pr = json_get_str(in, "project_root", "");
    bool source_mode = json_get_bool(in, "source_mode", false);
    json_t *pe = json_obj_get(in, "packaged_executable");
    json_t *r = NULL;
    char *dist = posix_join2(dd, "dist");

    if (source_mode) {
        json_t *a = field_str("desktop_dir", dd);
        json_t *de = mdf_call(io, "_desktop_dist_exists", a);
        json_free(a);
        if (mdf_raised(de)) { free(dist); return propagate(de); }
        bool exists = jtruth(de);
        json_free(de);
        if (!exists) {
            size_t cap = strlen(dist) + 96;
            char *msg = malloc(cap);
            snprintf(msg, cap,
                     "\xe2\x9c\x97 --skip-build --source was passed but no desktop dist found at: %s", dist);
            r = do_print(io, msg); free(msg);
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            r = do_print(io, "  Pre-build first:  cd apps/desktop && pnpm run build");
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            r = do_print(io, "  Or drop --skip-build to install dependencies and build automatically.");
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            free(dist);
            return exit_node(1);
        }
        a = field_str("project_root", pr);
        json_t *ed = mdf_call(io, "_electron_dir", a);
        json_free(a);
        if (mdf_raised(ed)) { free(dist); return propagate(ed); }
        char *pj = posix_join2(jstr(ed) ? jstr(ed) : "", "package.json");
        json_free(ed);
        a = field_str("path", pj);
        json_t *ex = mdf_call(io, "path.exists", a);
        json_free(a);
        free(pj);
        if (mdf_raised(ex)) { free(dist); return propagate(ex); }
        bool pkg = jtruth(ex);
        json_free(ex);
        if (!pkg) {
            r = do_print(io, "\xe2\x9c\x97 --skip-build --source requires existing desktop workspace dependencies.");
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            size_t cap = strlen(pr) + 96;
            char *msg = malloc(cap);
            snprintf(msg, cap, "  Install first:  cd %s && pnpm install --frozen-lockfile", pr);
            r = do_print(io, msg); free(msg);
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            r = do_print(io, "  Or drop --skip-build to install dependencies and build automatically.");
            if (mdf_raised(r)) { free(dist); return propagate(r); }
            json_free(r);
            free(dist);
            return exit_node(1);
        }
        size_t cap = strlen(dist) + 96;
        char *msg = malloc(cap);
        snprintf(msg, cap,
                 "\xe2\x86\x92 Skipping desktop source build (--skip-build --source); using dist at %s", dist);
        r = do_print(io, msg); free(msg);
        free(dist);
        if (mdf_raised(r)) return propagate(r);
        json_free(r);
        return json_null();
    }
    free(dist);
    if (j_is_null(pe)) {
        char *rel = posix_join2(dd, "release");
        size_t cap = strlen(rel) + 96;
        char *msg = malloc(cap);
        snprintf(msg, cap,
                 "\xe2\x9c\x97 --skip-build was passed but no packaged desktop app was found at: %s", rel);
        r = do_print(io, msg); free(msg); free(rel);
        if (mdf_raised(r)) return propagate(r);
        json_free(r);
        r = do_print(io, "  Pre-build first:  cd apps/desktop && pnpm run pack");
        if (mdf_raised(r)) return propagate(r);
        json_free(r);
        r = do_print(io, "  Or drop --skip-build to package automatically.");
        if (mdf_raised(r)) return propagate(r);
        json_free(r);
        return exit_node(1);
    }
    const char *pes = jstr(pe) ? jstr(pe) : "";
    size_t cap = strlen(pes) + 96;
    char *msg = malloc(cap);
    snprintf(msg, cap,
             "\xe2\x86\x92 Skipping desktop package build (--skip-build); using %s", pes);
    json_t *a = field_str("message", msg);
    free(msg);
    r = mdf_call(io, "desktop_launch_notice", a);
    json_free(a);
    if (mdf_raised(r)) return propagate(r);
    json_free(r);
    return json_null();
}

/* ── _packaged_desktop_launch_command ──────────────────────────────────── */
/* PoP: mdf_packaged_desktop_launch_command @ hermes_cli/main_desktop.py:_packaged_desktop_launch_command */
json_t *mdf_packaged_desktop_launch_command(mdf_io_t *io, json_t *in) {
    const char *pe = json_get_str(in, "packaged_executable", "");
    json_t *cmd = json_array();
    json_append(cmd, json_string(pe));
    json_t *a = field_str("packaged_executable", pe);
    json_t *fix = mdf_call(io, "_desktop_linux_sandbox_fixup", a);
    json_free(a);
    if (mdf_raised(fix)) { json_free(cmd); return propagate(fix); }
    bool fixed = jtruth(fix);
    json_free(fix);
    if (!fixed) {
        json_t *ns = mdf_call(io, "_desktop_linux_needs_no_sandbox", NULL);
        if (mdf_raised(ns)) { json_free(cmd); return propagate(ns); }
        bool needs = jtruth(ns);
        json_free(ns);
        bool regular = false;
        if (needs) {                              /* and short-circuit */
            a = field_str("packaged_executable", pe);
            json_t *reg = mdf_call(io, "_desktop_linux_sandbox_helper_is_regular_file", a);
            json_free(a);
            if (mdf_raised(reg)) { json_free(cmd); return propagate(reg); }
            regular = jtruth(reg);
            json_free(reg);
        }
        if (needs && regular) {
            a = field_str("packaged_executable", pe);
            json_t *fb = mdf_call(io, "print_no_sandbox_fallback", a);
            json_free(a);
            if (mdf_raised(fb)) { json_free(cmd); return propagate(fb); }
            json_free(fb);
            json_append(cmd, json_string("--no-sandbox"));
        } else {
            json_free(cmd);
            return exit_node(1);
        }
    } else {
        a = field_str("packaged_executable", pe);
        json_t *dis = mdf_call(io, "_desktop_linux_needs_disable_setuid_sandbox", a);
        json_free(a);
        if (mdf_raised(dis)) { json_free(cmd); return propagate(dis); }
        bool disable = jtruth(dis);
        json_free(dis);
        if (disable) json_append(cmd, json_string("--disable-setuid-sandbox"));
    }
    return cmd;
}

/* ── _run_desktop_one_shot ─────────────────────────────────────────────── */
/* PoP: mdf_run_desktop_one_shot @ hermes_cli/main_desktop.py:_run_desktop_one_shot */
json_t *mdf_run_desktop_one_shot(mdf_io_t *io, json_t *in) {
    const char *dd = json_get_str(in, "desktop_dir", "");
    bool bundled = json_get_bool(in, "bundled", false);
    json_t *args = json_obj_get(in, "args");

    if (arg_truthy(in, "setup_tcc_identity")) {
        json_t *idn = json_obj_get(args, "identity");
        const char *identity = jtruth(idn) && jstr(idn) ? jstr(idn) : "Hermes Local Signing";
        json_t *a = field_str("identity", identity);
        json_t *r = mdf_call(io, "_desktop_macos_setup_tcc_identity", a);
        json_free(a);
        if (mdf_raised(r)) return propagate(r);
        bool ok = jtruth(r);
        json_free(r);
        return exit_node(ok ? 0 : 1);
    }
    if (arg_truthy(in, "install_apparmor_profile")) {
        json_t *a = field_str("desktop_dir", dd);
        json_t *exe = mdf_call(io, "_desktop_packaged_executable", a);
        json_free(a);
        if (mdf_raised(exe)) return propagate(exe);
        if (bundled) {
            const char *pr = json_get_str(in, "project_root", "/repo");
            a = field_str("project_root", pr);
            json_t *rl = mdf_call(io, "resolve_bundle_layout", a);
            json_free(a);
            if (mdf_raised(rl)) {
                if (strcmp(exc_type(rl), "NotBundledApp") != 0) {
                    json_free(exe);
                    return propagate(rl);
                }
                json_free(rl);                    /* contextlib.suppress */
            } else {
                json_t *launcher = json_obj_get(rl, "launcher");
                json_free(exe);
                exe = launcher ? json_copy(launcher) : json_null();
                json_free(rl);
            }
        }
        if (j_is_null(exe)) {
            char *rel = posix_join2(dd, "release");
            size_t cap = strlen(rel) + 128;
            char *msg = malloc(cap);
            snprintf(msg, cap,
                     "\xe2\x9c\x97 No packaged Hermes Desktop found under %s; run `hermes desktop` first.", rel);
            json_t *r = do_print(io, msg);
            free(msg); free(rel);
            if (mdf_raised(r)) { json_free(exe); return propagate(r); }
            json_free(r);
        }
        bool ok = false;
        if (!j_is_null(exe)) {                    /* and short-circuit */
            a = field_str("exe", jstr(exe) ? jstr(exe) : "");
            json_t *ip = mdf_call(io, "install_profile", a);
            json_free(a);
            if (mdf_raised(ip)) { json_free(exe); return propagate(ip); }
            ok = jtruth(ip);
            json_free(ip);
        }
        json_free(exe);
        return exit_node(ok ? 0 : 1);
    }
    return json_null();
}

/* ── _launch_bundled_desktop ───────────────────────────────────────────── */
static char *join_strings(json_t *arr, const char *sep) {
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    size_t slen = strlen(sep);
    size_t n = j_is_arr(arr) ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstr(json_get(arr, i));
        if (!s) s = "";
        size_t sl = strlen(s);
        size_t need = len + sl + (i ? slen : 0) + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            out = realloc(out, cap);
        }
        if (i) { memcpy(out + len, sep, slen); len += slen; }
        memcpy(out + len, s, sl);
        len += sl;
        out[len] = '\0';
    }
    return out;
}
/* PoP: mdf_launch_bundled_desktop @ hermes_cli/main_desktop.py:_launch_bundled_desktop */
json_t *mdf_launch_bundled_desktop(mdf_io_t *io, json_t *in) {
    json_t *args = json_obj_get(in, "args");
    static const char *FLAGS[3] = { "--source", "--build-only", "--force-build" };
    static const char *NAMES[3] = { "source", "build_only", "force_build" };
    json_t *refused = json_array();
    for (int i = 0; i < 3; i++)
        if (jtruth(json_obj_get(args, NAMES[i]))) json_append(refused, json_string(FLAGS[i]));
    if (json_len(refused) > 0) {
        char *joined = join_strings(refused, ", ");
        size_t cap = strlen(joined) + 96;
        char *msg = malloc(cap);
        snprintf(msg, cap, "\xe2\x9c\x97 %s cannot apply to a bundled Hermes install.", joined);
        json_t *r = do_print(io, msg);
        free(msg); free(joined);
        if (mdf_raised(r)) { json_free(refused); return propagate(r); }
        json_free(r);
        r = do_print(io, "  This app ships prebuilt and has no desktop source tree to build.");
        if (mdf_raised(r)) { json_free(refused); return propagate(r); }
        json_free(r);
        json_free(refused);
        return exit_node(2);
    }
    json_free(refused);

    const char *pr = json_get_str(in, "project_root", "/repo");
    json_t *a = field_str("project_root", pr);
    json_t *layout = mdf_call(io, "resolve_bundle_layout", a);
    json_free(a);
    if (mdf_raised(layout)) {
        if (strcmp(exc_type(layout), "NotBundledApp") != 0) return propagate(layout);
        char *es = exc_str(layout);
        size_t cap = strlen(es) + 128;
        char *msg = malloc(cap);
        snprintf(msg, cap,
                 "\xe2\x9c\x97 This Hermes is stamped as a bundled desktop install, but %s.", es);
        json_t *r = do_print(io, msg);
        free(msg); free(es);
        if (mdf_raised(r)) { json_free(layout); return propagate(r); }
        json_free(r);
        r = do_print(io, "  The install is damaged \xe2\x80\x94 reinstall Hermes from the website.");
        if (mdf_raised(r)) { json_free(layout); return propagate(r); }
        json_free(r);
        json_free(layout);
        return exit_node(1);
    }
    const char *launcher = jstr(json_obj_get(layout, "launcher"));
    const char *app_root = json_get_str(layout, "app_root", "");
    if (!launcher) {
        size_t cap = strlen(app_root) + 96;
        char *msg = malloc(cap);
        snprintf(msg, cap, "\xe2\x9c\x97 Found no Hermes Desktop launcher in %s.", app_root);
        json_t *r = do_print(io, msg);
        free(msg);
        if (mdf_raised(r)) { json_free(layout); return propagate(r); }
        json_free(r);
        r = do_print(io, "  The install is damaged \xe2\x80\x94 reinstall Hermes from the website.");
        if (mdf_raised(r)) { json_free(layout); return propagate(r); }
        json_free(r);
        json_free(layout);
        return exit_node(1);
    }
    /* launch_command = _packaged_desktop_launch_command(layout.launcher) */
    json_t *sub = json_object();
    json_set_hdr(sub, "packaged_executable", launcher);
    json_t *cmd = mdf_packaged_desktop_launch_command(io, sub);
    json_free(sub);
    if (mdf_raised(cmd)) { json_free(layout); return propagate(cmd); }
    json_t *eflags = json_obj_get(in, "electron_flags");
    if (j_is_arr(eflags))
        for (size_t i = 0; i < json_len(eflags); i++)
            json_append(cmd, json_copy(json_get(eflags, i)));
    json_t *prof = mdf_explicit_profile_args(io, NULL);
    if (mdf_raised(prof)) { json_free(cmd); json_free(layout); return propagate(prof); }
    if (j_is_arr(prof))
        for (size_t i = 0; i < json_len(prof); i++)
            json_append(cmd, json_copy(json_get(prof, i)));
    json_free(prof);

    a = json_object();
    json_set(a, "launch_command", json_copy(cmd));
    json_t *env = json_obj_get(in, "env");
    json_set(a, "env", env ? json_copy(env) : json_object());
    json_set_hdr(a, "cwd", app_root);
    json_t *pid = mdf_call(io, "launch_detached", a);
    json_free(a);
    if (mdf_raised(pid)) { json_free(cmd); json_free(layout); return propagate(pid); }
    char pidbuf[64];
    if (pid && pid->type == JSON_NUMBER) {
        double v = pid->num_val;
        if (v == (double)(long long)v) snprintf(pidbuf, sizeof pidbuf, "%.0f", v);
        else snprintf(pidbuf, sizeof pidbuf, "%.17g", v);
    } else {
        snprintf(pidbuf, sizeof pidbuf, "%s", jstr(pid) ? jstr(pid) : "None");
    }
    json_free(pid);
    char *joined = join_strings(cmd, " ");
    size_t cap = strlen(joined) + strlen(pidbuf) + 96;
    char *msg = malloc(cap);
    snprintf(msg, cap, "\xe2\x86\x92 Launched Hermes Desktop: %s (pid %s)", joined, pidbuf);
    json_t *r = do_print(io, msg);
    free(msg); free(joined);
    json_free(cmd);
    json_free(layout);
    if (mdf_raised(r)) return propagate(r);
    json_free(r);
    return exit_node(0);
}

/* ── _rename_riding_out_file_lock ──────────────────────────────────────── */
/* PoP: mdf_rename_riding_out_file_lock @ hermes_cli/main_desktop.py:_rename_riding_out_file_lock */
json_t *mdf_rename_riding_out_file_lock(mdf_io_t *io, json_t *in) {
    const char *src = json_get_str(in, "src", "");
    const char *dst = json_get_str(in, "dst", "");
    static const double DELAYS[4] = { 0.5, 1.0, 1.0, 1.0 };
    char *src_name = posix_basename(src);
    char *dst_name = posix_basename(dst);
    json_t *result = NULL;

    for (int attempt = 1; attempt <= 4; attempt++) {
        json_t *a = json_object();
        json_set_hdr(a, "src", src);
        json_set_hdr(a, "dst", dst);
        json_t *r = mdf_call(io, "os.rename", a);
        json_free(a);
        if (!mdf_raised(r)) { json_free(r); result = json_null(); goto done; }
        if (strcmp(exc_type(r), "PermissionError") != 0) {
            result = propagate(r);                /* only PermissionError retries */
            goto done;
        }
        char *es = exc_str(r);
        json_free(r);
        size_t cap = strlen(src_name) + strlen(dst_name) + strlen(es) + 160;
        char *msg = malloc(cap);
        snprintf(msg, cap,
                 "desktop promotion rename %s -> %s hit a file lock (attempt %d/%d), retrying in %.1fs: %s",
                 src_name, dst_name, attempt, 5, DELAYS[attempt - 1], es);
        free(es);
        a = field_str("message", msg);
        free(msg);
        json_t *w = mdf_call(io, "logger.warning", a);
        json_free(a);
        if (mdf_raised(w)) { result = propagate(w); goto done; }
        json_free(w);
        a = json_object();
        json_set(a, "delay", json_number(DELAYS[attempt - 1]));
        json_t *sl = mdf_call(io, "time.sleep", a);
        json_free(a);
        if (mdf_raised(sl)) { result = propagate(sl); goto done; }
        json_free(sl);
    }
    json_t *a = json_object();
    json_set_hdr(a, "src", src);
    json_set_hdr(a, "dst", dst);
    json_t *r = mdf_call(io, "os.rename", a);
    json_free(a);
    result = mdf_raised(r) ? propagate(r) : (json_free(r), json_null());
done:
    free(src_name);
    free(dst_name);
    return result;
}
