/* main_desktop_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/main_desktop.py (the claimed siblings live in
 * port_main_desktop_flow.c and are SEAMED here).
 *
 * Decision-sequence port: stage-and-swap recovery helpers, macOS signing
 * chain (bundle id, local identity, codesign verify/summary, valid-real
 * predicate, legacy ad-hoc resign, inside-out local codesign), app.asar
 * hashing, macOS bundle install/refresh/launch-target flows, ownership
 * predicates, sandbox lstat, kernel32 loader, process ancestry predicates,
 * the staged-app promotion gate and build_prepared_desktop. ALL collaborator
 * I/O (claimed siblings, foreign-module imports, path predicates/reads/
 * renames, subprocess, shutil, psutil, plistlib, ctypes, logger, print) rides
 * the injected universal seam md2_call; the harness scripts results and
 * records the ordered trace the oracle diffs against LIVE
 * hermes_cli/main_desktop.py under the same scripts. Ported functions call
 * each other REAL (internal composition: _desktop_ancestor_in -> _runs_from,
 * _macos_signature_summary -> _codesign_verify, _desktop_macos_local_codesign
 * -> _desktop_macos_bundle_id/_codesign_verify, _install_rebuilt_desktop_app
 * -> _install_rebuilt_macos_bundles -> _recover_interrupted_swap/
 * _app_asar_hash/_macos_signature_summary/_stage_macos_bundle_copy/
 * _swap_in_new_macos_bundle, _refresh_installed_desktop_apps ->
 * _owns_installed_desktop_apps/_installed_desktop_apps ->
 * _update_owned_macos_bundles, _installed_desktop_launch_target ->
 * _refresh_installed_desktop_apps/_app_asar_hash/_installed_desktop_apps,
 * _promote_staged_desktop_app -> _discard_desktop_staging,
 * build_prepared_desktop -> _desktop_ancestor_in/_promote_staged_desktop_app/
 * _discard_desktop_staging).
 *
 * Representation contract (shared with tests/oracle_main_desktop_flow2.py):
 *   - Paths are normalized POSIX strings (no "."/".." components, no
 *     duplicate or trailing slashes, no double-leading-slash roots, no
 *     non-UTF-8 texts). Path -> str; tuple/set -> array (sets sorted
 *     ascending, deduped); None -> JSON null; integral floats collapse to
 *     ints on both sides.
 *   - uncaught exception -> {"__raise__": cls, "message": str(exc)};
 *     SystemExit -> {"__raise__": "SystemExit", "code": ...}. Scripted
 *     OSError with errno promotes to the errno subclass (CPython
 *     OSError(errno, msg) constructor semantics) on BOTH sides.
 *   - scripts: a plain JSON value is reused verbatim; a JSON ARRAY is a
 *     per-call queue (one element per call); {"__script__": true, "ret": v,
 *     "raise": {type, message, errno?, code?, returncode?, cmd?, output?}}
 *     returns v or raises. A missing/exhausted script is a fail-closed
 *     RuntimeError("no script for <name>" / "script exhausted for <name>")
 *     recorded in the trace BEFORE propagation on both sides.
 *   - except Exception never catches the BaseException trio
 *     (SystemExit/KeyboardInterrupt/GeneratorExit) nor BaseExceptionGroup —
 *     those propagate. Catch matching walks the FULL builtin exception
 *     hierarchy (py3.14.9 probe) plus the stdlib kinds used here
 *     (SubprocessError/CalledProcessError/TimeoutExpired/JSONDecodeError).
 *   - print is an echo/effect seam: a script entry is required as a marker;
 *     args.text carries the FULL written text (end newline included); the
 *     harness appends it to the real stdout/stderr capture buffers.
 *   - subprocess.run seam args = {"argv": [...], "capture_output": true,
 *     **kwargs}; result {"returncode", stdout, stderr} (str in text mode,
 *     null otherwise — bytes-mode outputs are never read by these flows).
 *     check=True + nonzero returncode raises the REAL CalledProcessError
 *     semantics on both sides: str = "Command '<repr(argv)>' returned
 *     non-zero exit status <rc>." with output/stderr carried.
 *   - shutil.rmtree(ignore_errors=True) swallows OSError-family seam raises
 *     on both sides; other kinds propagate.
 *   - path.read_text(encoding="utf-8-sig") strips a leading BOM on both
 *     sides AFTER the seam (the trace carries the raw script).
 *   - file.read / path.read_bytes / plistlib.loads scripts carry hex-encoded
 *     bytes; the SHA-256 is computed FOR REAL in C (md2_sha256, libjson-only
 *     port — no libhash dependency).
 *   - json.loads is ported FOR REAL with a strict JSON grammar pre-validator
 *     (libjson json_parse is lenient); the invalid-text fixture domain is
 *     empty/whitespace-only stamps (JSONDecodeError "Expecting value: line 1
 *     column C (char P)"). NaN/Infinity are out of domain.
 *   - os.walk seam returns the scripted [{"root","dirs","files"}] sequence;
 *     standalone/bundle sorts are STABLE by len(parts) descending. The
 *     nested-bundle set fixture domain has DISTINCT part counts (live set
 *     iteration order is hash-randomized on ties).
 *   - _swap_staged_desktop_app seam ret = {"exe": path|null, "held_by":
 *     [names]} — the held_by list mutation the live sibling performs is
 *     modeled by the held_by field (both sides extend the caller list).
 *   - release_after_refused_rename seam ret = {"holders": [...], "closed":
 *     bool} projects to the live (holders, closed) tuple.
 *   - _force_adhoc_macos_signing seam: when it returns True the live sibling
 *     sets env["CSC_IDENTITY_AUTO_DISCOVERY"]="false"; both sides apply that
 *     single effect to the passed env dict.
 *   - _kernel32 seam ctypes.WinDLL ret is an opaque marker object; the live
 *     WinDLL double projects back to the same marker.
 *   - proc doubles: {"id", "pid"} records; proc.exe / proc.parents seams are
 *     keyed by id. psutil.process_iter(["exe"]) ret = [{"exe": str|null}].
 *   - str()/repr() fixture scope is ASCII; str.strip() strips the ASCII
 *     White_Space set (incl \x1c-\x1f); splitlines boundaries are
 *     \n \r \r\n \v \f \x1c \x1d \x1e (NOT \x1f).
 *   - in.platform models sys.platform (default "linux").
 *   - path.resolve is an IDENTITY ECHO seam (fixture paths are canonical):
 *     the dispatcher records {"path"} and returns the input path; a script
 *     marker is required and scripted raises are honored. No queue accounting.
 *
 * libjson-only (gate: run_one_oracle.sh main_desktop_flow2
 * oracle_main_desktop_flow2.py "" "lib/libjson/json.o"). Self-contained:
 * the harness #includes "cli/main_desktop_flow2.c". Everything is static
 * md2_* (the md2_ prefix is shared with models_flow2.c but both ports are
 * fully static and never co-linked).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── context + seam plumbing ───────────────────────────────────────────── */

typedef struct md2_io_t {
    json_t *(*call)(struct md2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"name","args"}: ordered seam sequence */
    void   *ctx;     /* harness backend (scripted results + state) */
} md2_io_t;

/* CPython OSError(errno, msg) promotes the BASE OSError to the errno's
 * subclass; a subclass constructor does NOT re-promote. Mirror at seam
 * materialization (the oracle builds real OSError(errno, msg)). */
static void md2_promote_oserror(json_t *r) {
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

/* Universal seam: record {"name","args"} (args borrowed, copied), then
 * dispatch. A NULL call channel returns None (production fail-closed rides
 * the harness dispatcher's no-script RuntimeError). */
static json_t *md2_call(md2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    json_t *r = io->call(io, name, args);
    md2_promote_oserror(r);
    return r;
}

static bool md2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *md2_raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message ? message : "");
    return o;
}

/* ── Python truthiness / type tests ────────────────────────────────────── */

static bool md2_truth(const json_t *v) {
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
static bool md2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool md2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool md2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool md2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool md2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool md2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *md2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ── exception hierarchy (FULL builtin tree, py3.14.9 probe) ───────────── */

typedef struct { const char *name; const char *parent; } md2_exc_ent;
static const md2_exc_ent MD2_EXC_TREE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"NotShareableError", "TypeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"ZipImportError", "ImportError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"UnsupportedOperation", "OSError"},
    {"EOFError", "Exception"},
    {"RuntimeError", "Exception"},
    {"RecursionError", "RuntimeError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"BrokenBarrierError", "RuntimeError"},
    {"_DeadlockError", "RuntimeError"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"AttributeError", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"CodecRegistryError", "LookupError"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"AssertionError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"SystemError", "Exception"},
    {"ReferenceError", "Exception"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"Warning", "Exception"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"Error", "Exception"},
    {"PatternError", "Exception"},
    {"InterpreterError", "Exception"},
    {"InterpreterNotFoundError", "InterpreterError"},
    {"_OptionError", "Exception"},
    {"SubprocessError", "Exception"},
    {"CalledProcessError", "SubprocessError"},
    {"TimeoutExpired", "SubprocessError"},
};

static const char *md2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(MD2_EXC_TREE)/sizeof(MD2_EXC_TREE[0]); i++)
        if (strcmp(MD2_EXC_TREE[i].name, kind) == 0) return MD2_EXC_TREE[i].parent;
    return "Exception";   /* synthetic scripted kinds are Exception subclasses */
}

/* isinstance(exc, base) by name walk. */
static bool md2_exc_is_a(const char *kind, const char *base) {
    if (!kind) return false;
    const char *cur = kind;
    while (cur) {
        if (strcmp(cur, base) == 0) return true;
        cur = md2_exc_parent(cur);
    }
    return false;
}

static const char *md2_exc_type(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* except Exception catchability (BaseException trio + groups pass through). */
static bool md2_exc_is_exception(const json_t *r) {
    return md2_exc_is_a(md2_exc_type(r), "Exception");
}

/* str(exc) parity: OSError(errno, msg) -> "[Errno N] msg"; else msg. */
static char *md2_exc_str(const json_t *r) {
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
static json_t *md2_propagate(json_t *r) {
    if (!md2_raised(r)) return r;
    if (strcmp(md2_exc_type(r), "SystemExit") == 0) {
        json_t *code = json_obj_get(r, "code");
        json_t *o = json_object();
        json_set_hdr(o, "__raise__", "SystemExit");
        json_set(o, "code", code ? json_copy(code) : json_null());
        json_free(r);
        return o;
    }
    char *m = md2_exc_str(r);
    json_t *o = md2_raise_desc(md2_exc_type(r), m);
    free(m);
    json_free(r);
    return o;
}
/* ── string builder ────────────────────────────────────────────────────── */

typedef struct { char *s; size_t len, cap; } md2_sbuf;
static void md2_sb_init(md2_sbuf *b) { b->s = NULL; b->len = 0; b->cap = 0; }
static void md2_sb_add(md2_sbuf *b, const char *text) {
    if (!text) text = "";
    size_t n = strlen(text);
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (nc < b->len + n + 1) nc *= 2;
        b->s = realloc(b->s, nc);
        b->cap = nc;
    }
    memcpy(b->s + b->len, text, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void md2_sb_addn(md2_sbuf *b, const char *text, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (nc < b->len + n + 1) nc *= 2;
        b->s = realloc(b->s, nc);
        b->cap = nc;
    }
    memcpy(b->s + b->len, text, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void md2_sb_addnum(md2_sbuf *b, double v) {
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%.0f", v);
    md2_sb_add(b, tmp);
}
static char *md2_sb_take(md2_sbuf *b) {
    char *s = b->s ? b->s : malloc(1);
    if (!b->s) s[0] = '\0';
    b->s = NULL; b->len = b->cap = 0;
    return s;
}
static void md2_sb_free(md2_sbuf *b) { free(b->s); b->s = NULL; }

static char *md2_dupn(const char *s, size_t n) {
    char *o = malloc(n + 1);
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *md2_dup(const char *s) { return md2_dupn(s ? s : "", strlen(s ? s : "")); }

/* ── ASCII str helpers (fixture scope: ASCII; see header) ──────────────── */

static bool md2_is_ws(unsigned char c) {
    /* Python str.strip() White_Space, ASCII subset (incl \x1c-\x1f). */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v'
        || c == '\f' || (c >= 0x1c && c <= 0x1f);
}
static char *md2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && md2_is_ws((unsigned char)s[a])) a++;
    while (b > a && md2_is_ws((unsigned char)s[b - 1])) b--;
    return md2_dupn(s + a, b - a);
}
static char *md2_lower(const char *s) {
    char *o = md2_dup(s);
    for (char *p = o; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return o;
}
static bool md2_endswith(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return m <= n && memcmp(s + n - m, suf, m) == 0;
}
static bool md2_startswith(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}

/* Python str repr() for the ASCII fixture scope: ' preferred, " when the
 * text contains ' but no "; backslash/quote/\n/\r/\t/control escapes. */
static char *md2_repr_str(const char *s) {
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

/* Python repr() / str() of a JSON value (ASCII fixture domain). */
static char *md2_pyrepr(const json_t *v);
static char *md2_pystr(const json_t *v) {
    if (md2_is_str(v)) return md2_dup(v->str_val);
    return md2_pyrepr(v);
}
static char *md2_pyrepr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return md2_dup("None");
    if (v->type == JSON_BOOL) return md2_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof tmp, "%.0f", v->num_val);
        else
            snprintf(tmp, sizeof tmp, "%g", v->num_val);
        return md2_dup(tmp);
    }
    if (v->type == JSON_STRING) return md2_repr_str(v->str_val);
    md2_sbuf b; md2_sb_init(&b);
    if (v->type == JSON_ARRAY) {
        md2_sb_add(&b, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) md2_sb_add(&b, ", ");
            char *r = md2_pyrepr(v->c.items[i]);
            md2_sb_add(&b, r);
            free(r);
        }
        md2_sb_add(&b, "]");
    } else {
        md2_sb_add(&b, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) md2_sb_add(&b, ", ");
            char *k = md2_repr_str(v->c.keys[i]);
            md2_sb_add(&b, k);
            free(k);
            md2_sb_add(&b, ": ");
            char *r = md2_pyrepr(v->c.items[i]);
            md2_sb_add(&b, r);
            free(r);
        }
        md2_sb_add(&b, "}");
    }
    return md2_sb_take(&b);
}

/* str.splitlines(): ASCII boundaries \n \r \r\n \v \f \x1c \x1d \x1e
 * (NOT \x1f). Returns a NEW array of line strings (no ends). */
static bool md2_is_linebreak(unsigned char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1e);
}
static json_t *md2_splitlines(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    size_t n = strlen(s), start = 0, i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (md2_is_linebreak(c)) {
            json_append(out, json_string(md2_dupn(s + start, i - start)));
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
            start = i;
        } else i++;
    }
    if (start < n) json_append(out, json_string(md2_dupn(s + start, n - start)));
    return out;
}

/* ── PurePosixPath lexical model (normalized fixture domain) ───────────── */

/* posixpath.join(a, b) */
static char *md2_pjoin(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return md2_dup(b);
    if (!a[0]) return md2_dup(b);
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
/* PurePosixPath(s).parent */
static char *md2_pparent(const char *s) {
    if (!s || !s[0] || strcmp(s, ".") == 0) return md2_dup(".");
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if (n == 1 && s[0] == '/') return md2_dup("/");
    size_t cut = n;
    while (cut > 0 && s[cut - 1] != '/') cut--;
    if (cut == 0) return md2_dup(".");
    if (cut == 1) return md2_dup("/");
    return md2_dupn(s, cut - 1);
}
/* PurePosixPath(s).parents — tuple of ancestors (root/"." terminate). */
static json_t *md2_pparents(const char *s) {
    json_t *out = json_array();
    char *cur = md2_dup(s ? s : "");
    for (;;) {
        char *p = md2_pparent(cur);
        if (strcmp(p, cur) == 0) { free(p); break; }
        json_append(out, json_string(p));
        free(cur);
        cur = p;
    }
    free(cur);
    return out;
}
/* PurePosixPath(s).parts — components; absolute paths lead with "/". */
static json_t *md2_pparts(const char *s) {
    json_t *out = json_array();
    if (!s || !s[0] || strcmp(s, ".") == 0) return out;
    if (s[0] == '/') json_append(out, json_string("/"));
    const char *p = s;
    while (*p) {
        while (*p == '/') p++;
        const char *start = p;
        while (*p && *p != '/') p++;
        if (p > start) json_append(out, json_string(md2_dupn(start, (size_t)(p - start))));
    }
    return out;
}
/* PurePosixPath(s).name */
static char *md2_pname(const char *s) {
    if (!s || !s[0]) return md2_dup("");
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if (n == 1 && s[0] == '/') return md2_dup("");
    size_t start = n;
    while (start > 0 && s[start - 1] != '/') start--;
    return md2_dupn(s + start, n - start);
}
/* PurePosixPath(s).suffix */
static char *md2_psuffix(const char *s) {
    char *name = md2_pname(s);
    size_t n = strlen(name);
    size_t i = n;
    while (i > 0 && name[i - 1] != '.') i--;
    /* i is index AFTER the last dot (0 when no dot). */
    if (i == 0 || i == 1) { free(name); return md2_dup(""); }
    char *o = md2_dup(name + i - 1);
    free(name);
    return o;
}
/* PurePosixPath(s).relative_to(base).parts — ValueError when not a prefix.
 * Returns a NEW parts array, or a raise descriptor on mismatch. */
static json_t *md2_relative_parts(const char *s, const char *base) {
    json_t *sp = md2_pparts(s);
    json_t *bp = md2_pparts(base);
    bool ok = json_len(bp) <= json_len(sp);
    if (ok) {
        for (size_t i = 0; i < json_len(bp); i++) {
            if (strcmp(json_get(sp, i)->str_val, json_get(bp, i)->str_val) != 0) {
                ok = false;
                break;
            }
        }
    }
    json_t *out = json_array();
    if (ok) {
        for (size_t i = json_len(bp); i < json_len(sp); i++)
            json_append(out, json_string(json_get(sp, i)->str_val));
    } else {
        json_free(out);
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "'");
        md2_sb_add(&b, s);
        md2_sb_add(&b, "' is not in the subpath of '");
        md2_sb_add(&b, base);
        md2_sb_add(&b, "'");
        char *m = md2_sb_take(&b);
        out = md2_raise_desc("ValueError", m);
        free(m);
    }
    json_free(sp);
    json_free(bp);
    return out;
}
/* Path(v) constructor type gate: non-str raises the CPython TypeError. */
static json_t *md2_path_ctor(const json_t *v) {
    if (md2_is_str(v)) return json_string(v->str_val);
    const char *tn = "NoneType";
    if (v) {
        switch (v->type) {
            case JSON_BOOL: tn = "bool"; break;
            case JSON_NUMBER: tn = "int"; break;
            case JSON_ARRAY: tn = "list"; break;
            case JSON_OBJECT: tn = "dict"; break;
            default: tn = "NoneType"; break;
        }
    }
    md2_sbuf b; md2_sb_init(&b);
    md2_sb_add(&b, "argument should be a str or an os.PathLike object where "
                   "__fspath__ returns a str, not '");
    md2_sb_add(&b, tn);
    md2_sb_add(&b, "'");
    char *m = md2_sb_take(&b);
    json_t *o = md2_raise_desc("TypeError", m);
    free(m);
    return o;
}
/* ── SHA-256 (FIPS 180-4; real computation, libjson-only port) ─────────── */

typedef struct {
    unsigned int h[8];
    unsigned long long total;
    unsigned char buf[64];
    size_t buflen;
} md2_sha256_ctx;

static const unsigned int MD2_SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static unsigned int md2_rotr(unsigned int x, unsigned int n) {
    return (x >> n) | (x << (32 - n));
}

static void md2_sha256_block(md2_sha256_ctx *c, const unsigned char *p) {
    unsigned int w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((unsigned int)p[i*4] << 24) | ((unsigned int)p[i*4+1] << 16)
             | ((unsigned int)p[i*4+2] << 8) | (unsigned int)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        unsigned int s0 = md2_rotr(w[i-15],7) ^ md2_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        unsigned int s1 = md2_rotr(w[i-2],17) ^ md2_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    unsigned int a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],
                 e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        unsigned int S1 = md2_rotr(e,6) ^ md2_rotr(e,11) ^ md2_rotr(e,25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int t1 = h + S1 + ch + MD2_SHA256_K[i] + w[i];
        unsigned int S0 = md2_rotr(a,2) ^ md2_rotr(a,13) ^ md2_rotr(a,22);
        unsigned int maj = (a & b) ^ (a & cc) ^ (b & cc);
        unsigned int t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void md2_sha256_init(md2_sha256_ctx *c) {
    c->h[0]=0x6a09e667; c->h[1]=0xbb67ae85; c->h[2]=0x3c6ef372; c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f; c->h[5]=0x9b05688c; c->h[6]=0x1f83d9ab; c->h[7]=0x5be0cd19;
    c->total = 0; c->buflen = 0;
}
static void md2_sha256_update(md2_sha256_ctx *c, const unsigned char *data, size_t n) {
    c->total += n;
    while (n > 0) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        n -= take;
        if (c->buflen == 64) {
            md2_sha256_block(c, c->buf);
            c->buflen = 0;
        }
    }
}
static char *md2_sha256_final(md2_sha256_ctx *c) {
    unsigned long long bits = c->total * 8;
    unsigned char pad = 0x80;
    md2_sha256_update(c, &pad, 1);
    unsigned char zero = 0;
    while (c->buflen != 56) md2_sha256_update(c, &zero, 1);
    unsigned char lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - 8*i));
    md2_sha256_update(c, lenb, 8);
    char *out = malloc(65);
    for (int i = 0; i < 8; i++)
        snprintf(out + i*8, 9, "%08x", c->h[i]);
    out[64] = '\0';
    return out;
}

/* hex string -> bytes (script domain: valid even-length hex). */
static unsigned char *md2_hex_decode(const char *hex, size_t *outlen) {
    size_t n = hex ? strlen(hex) : 0;
    unsigned char *o = malloc(n / 2 + 1);
    size_t k = 0;
    for (size_t i = 0; i + 1 < n; i += 2) {
        unsigned int b;
        if (sscanf(hex + i, "%2x", &b) != 1) b = 0;
        o[k++] = (unsigned char)b;
    }
    *outlen = k;
    return o;
}

/* ── strict json.loads (grammar pre-validator + libjson parse) ─────────── */

static bool md2_json_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
typedef struct { const char *s; size_t n, i; bool ok; } md2_jv;
static void md2_jv_skip(md2_jv *v) {
    while (v->i < v->n && md2_json_ws(v->s[v->i])) v->i++;
}
static void md2_jv_value(md2_jv *v);
static void md2_jv_string(md2_jv *v) {
    /* assumes s[i] == '"' */
    v->i++;
    while (v->i < v->n) {
        char c = v->s[v->i];
        if (c == '"') { v->i++; return; }
        if (c == '\\') {
            v->i++;
            if (v->i >= v->n) { v->ok = false; return; }
            char e = v->s[v->i];
            if (e == 'u') {
                for (int k = 0; k < 4; k++) {
                    v->i++;
                    if (v->i >= v->n) { v->ok = false; return; }
                    char h = v->s[v->i];
                    bool hexd = (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f')
                             || (h >= 'A' && h <= 'F');
                    if (!hexd) { v->ok = false; return; }
                }
                v->i++;
            } else if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f'
                       || e == 'n' || e == 'r' || e == 't') {
                v->i++;
            } else { v->ok = false; return; }
        } else if ((unsigned char)c < 0x20) { v->ok = false; return; }
        else v->i++;
    }
    v->ok = false;   /* unterminated */
}
static void md2_jv_number(md2_jv *v) {
    size_t start = v->i;
    if (v->i < v->n && v->s[v->i] == '-') v->i++;
    if (v->i >= v->n) { v->ok = false; v->i = start; return; }
    if (v->s[v->i] == '0') {
        v->i++;
        if (v->i < v->n && v->s[v->i] >= '0' && v->s[v->i] <= '9') {
            v->ok = false; v->i = start; return;   /* leading zero */
        }
    } else if (v->s[v->i] >= '1' && v->s[v->i] <= '9') {
        while (v->i < v->n && v->s[v->i] >= '0' && v->s[v->i] <= '9') v->i++;
    } else { v->ok = false; v->i = start; return; }
    if (v->i < v->n && v->s[v->i] == '.') {
        v->i++;
        size_t f = v->i;
        while (v->i < v->n && v->s[v->i] >= '0' && v->s[v->i] <= '9') v->i++;
        if (v->i == f) { v->ok = false; v->i = start; return; }
    }
    if (v->i < v->n && (v->s[v->i] == 'e' || v->s[v->i] == 'E')) {
        v->i++;
        if (v->i < v->n && (v->s[v->i] == '+' || v->s[v->i] == '-')) v->i++;
        size_t f = v->i;
        while (v->i < v->n && v->s[v->i] >= '0' && v->s[v->i] <= '9') v->i++;
        if (v->i == f) { v->ok = false; v->i = start; return; }
    }
}
static void md2_jv_value(md2_jv *v) {
    md2_jv_skip(v);
    if (v->i >= v->n) { v->ok = false; return; }
    char c = v->s[v->i];
    if (c == '"') { md2_jv_string(v); return; }
    if (c == '-' || (c >= '0' && c <= '9')) { md2_jv_number(v); return; }
    if (c == '{') {
        v->i++;
        md2_jv_skip(v);
        if (v->i < v->n && v->s[v->i] == '}') { v->i++; return; }
        for (;;) {
            md2_jv_skip(v);
            if (v->i >= v->n || v->s[v->i] != '"') { v->ok = false; return; }
            md2_jv_string(v);
            if (!v->ok) return;
            md2_jv_skip(v);
            if (v->i >= v->n || v->s[v->i] != ':') { v->ok = false; return; }
            v->i++;
            md2_jv_value(v);
            if (!v->ok) return;
            md2_jv_skip(v);
            if (v->i < v->n && v->s[v->i] == ',') { v->i++; continue; }
            if (v->i < v->n && v->s[v->i] == '}') { v->i++; return; }
            v->ok = false; return;
        }
    }
    if (c == '[') {
        v->i++;
        md2_jv_skip(v);
        if (v->i < v->n && v->s[v->i] == ']') { v->i++; return; }
        for (;;) {
            md2_jv_value(v);
            if (!v->ok) return;
            md2_jv_skip(v);
            if (v->i < v->n && v->s[v->i] == ',') { v->i++; continue; }
            if (v->i < v->n && v->s[v->i] == ']') { v->i++; return; }
            v->ok = false; return;
        }
    }
    if (v->n - v->i >= 4 && memcmp(v->s + v->i, "true", 4) == 0) { v->i += 4; return; }
    if (v->n - v->i >= 5 && memcmp(v->s + v->i, "false", 5) == 0) { v->i += 5; return; }
    if (v->n - v->i >= 4 && memcmp(v->s + v->i, "null", 4) == 0) { v->i += 4; return; }
    v->ok = false;
}
/* json.loads(text): strict grammar check, then libjson parse. The invalid
 * fixture domain is empty/whitespace-only text -> JSONDecodeError
 * "Expecting value: line 1 column C (char P)" (single-line stamps). */
static json_t *md2_json_loads(const char *text) {
    md2_jv v = { text ? text : "", strlen(text ? text : ""), 0, true };
    md2_jv_value(&v);
    if (v.ok) {
        md2_jv_skip(&v);
        if (v.i != v.n) v.ok = false;
    }
    if (!v.ok) {
        size_t p = v.i;
        if (p > v.n) p = v.n;
        char msg[128];
        snprintf(msg, sizeof msg,
                 "Expecting value: line 1 column %zu (char %zu)", p + 1, p);
        return md2_raise_desc("JSONDecodeError", msg);
    }
    json_t *r = json_parse(text, NULL);
    return r ? r : json_null();
}

/* ── seam wrappers (each returns a NEW node; raise descriptors pass) ───── */

/* builtins.print(text): args.text carries the end newline (flow1 contract). */
static json_t *md2_print(md2_io_t *io, const char *text) {
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
    json_t *r = md2_call(io, "print", a);
    json_free(a);
    return r;
}
static json_t *md2_log_warning(md2_io_t *io, const char *message) {
    json_t *a = json_object();
    json_set_hdr(a, "message", message);
    json_t *r = md2_call(io, "logger.warning", a);
    json_free(a);
    return r;
}
static json_t *md2_path_pred(md2_io_t *io, const char *name, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_t *r = md2_call(io, name, a);
    json_free(a);
    return r;
}
static json_t *md2_path_rename(md2_io_t *io, const char *path, const char *target) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "target", target);
    json_t *r = md2_call(io, "path.rename", a);
    json_free(a);
    return r;
}
static json_t *md2_path_resolve(md2_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_t *r = md2_call(io, "path.resolve", a);
    json_free(a);
    return r;
}
static json_t *md2_path_glob(md2_io_t *io, const char *path, const char *pattern) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "pattern", pattern);
    json_t *r = md2_call(io, "path.glob", a);
    json_free(a);
    return r;
}
/* read_text: utf-8-sig strips a leading BOM AFTER the seam (both sides). */
static json_t *md2_path_read_text(md2_io_t *io, const char *path, const char *encoding) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "encoding", encoding);
    json_t *r = md2_call(io, "path.read_text", a);
    json_free(a);
    if (md2_is_str(r) && strcmp(encoding, "utf-8-sig") == 0
        && (unsigned char)r->str_val[0] == 0xef
        && (unsigned char)r->str_val[1] == 0xbb
        && (unsigned char)r->str_val[2] == 0xbf) {
        memmove(r->str_val, r->str_val + 3, strlen(r->str_val + 3) + 1);
    }
    return r;
}
static json_t *md2_path_read_bytes(md2_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_t *r = md2_call(io, "path.read_bytes", a);
    json_free(a);
    return r;
}
/* shutil.rmtree(path, ignore_errors=True): OSError-family raises swallowed. */
static json_t *md2_rmtree(md2_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set(a, "ignore_errors", json_bool(true));
    json_t *r = md2_call(io, "shutil.rmtree", a);
    json_free(a);
    if (md2_raised(r) && md2_exc_is_a(md2_exc_type(r), "OSError")) {
        json_free(r);
        return json_null();
    }
    return r;
}
static json_t *md2_which(md2_io_t *io, const char *name) {
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_t *r = md2_call(io, "shutil.which", a);
    json_free(a);
    return r;
}
/* subprocess.run(argv, capture_output=True, **kwargs). check=True + nonzero
 * rc raises CalledProcessError with the CPython str() format. */
static json_t *md2_subprocess_run(md2_io_t *io, const json_t *argv, const json_t *kwargs) {
    json_t *a = json_object();
    json_set(a, "argv", json_copy(argv));
    json_set(a, "capture_output", json_bool(true));
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(a, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    json_t *r = md2_call(io, "subprocess.run", a);
    json_free(a);
    if (md2_raised(r)) return r;
    bool check = kwargs && md2_truth(json_obj_get(kwargs, "check"));
    double rc = json_get_num(r, "returncode", 0);
    if (check && rc != 0) {
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "Command '");
        md2_sb_add(&b, "[");
        for (size_t i = 0; i < json_len(argv); i++) {
            if (i) md2_sb_add(&b, ", ");
            const json_t *el = json_get(argv, i);
            char *r2 = md2_repr_str(md2_is_str(el) ? el->str_val : "");
            md2_sb_add(&b, r2);
            free(r2);
        }
        md2_sb_add(&b, "]");
        md2_sb_add(&b, "' returned non-zero exit status ");
        md2_sb_addnum(&b, rc);
        md2_sb_add(&b, ".");
        char *m = md2_sb_take(&b);
        json_t *e = md2_raise_desc("CalledProcessError", m);
        free(m);
        json_set(e, "returncode", json_number(rc));
        json_set(e, "cmd", json_copy(argv));
        json_t *out = json_obj_get(r, "stdout");
        json_set(e, "output", out ? json_copy(out) : json_null());
        json_t *err = json_obj_get(r, "stderr");
        json_set(e, "stderr", err ? json_copy(err) : json_null());
        json_free(r);
        return e;
    }
    return r;
}
static json_t *md2_open_rb(md2_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set_hdr(a, "mode", "rb");
    json_t *r = md2_call(io, "open", a);
    json_free(a);
    return r;
}
static json_t *md2_file_read(md2_io_t *io, const char *path, double n) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_set(a, "n", json_number(n));
    json_t *r = md2_call(io, "file.read", a);
    json_free(a);
    return r;
}
static json_t *md2_os_walk(md2_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set_hdr(a, "path", path);
    json_t *r = md2_call(io, "os.walk", a);
    json_free(a);
    return r;
}
static json_t *md2_getpid(md2_io_t *io) {
    json_t *a = json_object();
    json_t *r = md2_call(io, "os.getpid", a);
    json_free(a);
    return r;
}
/* ── live message constants (UTF-8 bytes; ASCII fixture scope) ─────────── */

/* _PREVIOUS_APP_KEPT = "  ↩ The previous desktop app was left untouched and still works." */
#define MD2_KEPT "  " "\xe2\x86\xa9" " The previous desktop app was left untouched and still works."
#define MD2_ARROW "\xe2\x86\x92"
#define MD2_WARN "\xe2\x9a\xa0"
#define MD2_CHECK "\xe2\x9c\x93"
#define MD2_CROSS "\xe2\x9c\x97"

/* Propagate a raised seam result (ownership transfers); value results pass
 * through unchanged. Use only when returning the node itself. */
#define MD2_PROP(r) do { if (md2_raised(r)) return md2_propagate(r); } while (0)

/* ── _packaged_resources_dir ───────────────────────────────────────────── */
/* PoP: md2_packaged_resources_dir @ hermes_cli/main_desktop.py:_packaged_resources_dir */
static json_t *md2_packaged_resources_dir(md2_io_t *io, json_t *in) {
    const char *desktop_dir = json_get_str(in, "desktop_dir", "");
    json_t *a = json_object();
    json_set_hdr(a, "desktop_dir", desktop_dir);
    json_t *exe = md2_call(io, "_desktop_packaged_executable", a);
    json_free(a);
    MD2_PROP(exe);
    if (md2_is_none(exe)) { json_free(exe); return json_null(); }
    const char *ex = md2_jstr(exe);
    char *p1 = md2_pparent(ex);
    char *res;
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") == 0) {
        char *p2 = md2_pparent(p1);
        res = md2_pjoin(p2, "Resources");
        free(p2);
    } else {
        res = md2_pjoin(p1, "resources");
    }
    free(p1);
    json_free(exe);
    json_t *out = json_string(res);
    free(res);
    return out;
}

/* ── _recover_interrupted_swap ─────────────────────────────────────────── */
/* PoP: md2_recover_interrupted_swap @ hermes_cli/main_desktop.py:_recover_interrupted_swap */
static json_t *md2_recover_interrupted_swap(md2_io_t *io, json_t *in) {
    const char *live = json_get_str(in, "live", "");
    const char *aside = json_get_str(in, "aside", "");
    json_t *ex = md2_path_pred(io, "path.exists", live);
    MD2_PROP(ex);
    bool live_exists = md2_truth(ex);
    json_free(ex);
    if (!live_exists) {
        json_t *isd = md2_path_pred(io, "path.is_dir", aside);
        MD2_PROP(isd);
        bool aside_dir = md2_truth(isd);
        json_free(isd);
        if (aside_dir) {
            json_t *rn = md2_path_rename(io, aside, live);
            if (!md2_raised(rn)) {
                json_free(rn);
                md2_sbuf b; md2_sb_init(&b);
                md2_sb_add(&b, "restored ");
                md2_sb_add(&b, live);
                md2_sb_add(&b, " from an interrupted desktop app swap");
                char *m = md2_sb_take(&b);
                json_t *lw = md2_log_warning(io, m);
                free(m);
                MD2_PROP(lw);
                json_free(lw);
                return json_null();
            }
            if (md2_exc_is_a(md2_exc_type(rn), "OSError")) {
                char *es = md2_exc_str(rn);
                md2_sbuf b; md2_sb_init(&b);
                md2_sb_add(&b, "could not restore ");
                md2_sb_add(&b, live);
                md2_sb_add(&b, " from ");
                md2_sb_add(&b, aside);
                md2_sb_add(&b, ": ");
                md2_sb_add(&b, es);
                free(es);
                char *m = md2_sb_take(&b);
                json_free(rn);
                json_t *lw = md2_log_warning(io, m);
                free(m);
                MD2_PROP(lw);
                json_free(lw);
                return json_null();
            }
            return md2_propagate(rn);
        }
    }
    json_t *rm = md2_rmtree(io, aside);
    MD2_PROP(rm);
    json_free(rm);
    return json_null();
}

/* ── _move_live_app_aside ──────────────────────────────────────────────── */
/* held_by (in.held_by, array or null) is mutated in place; the harness
 * echoes it as out.state.held_by. */
/* PoP: md2_move_live_app_aside @ hermes_cli/main_desktop.py:_move_live_app_aside */
static json_t *md2_move_live_app_aside(md2_io_t *io, json_t *in) {
    const char *live_root = json_get_str(in, "live_root", "");
    const char *previous = json_get_str(in, "previous", "");
    json_t *held_by = json_obj_get(in, "held_by");   /* borrowed; array or null */
    const char *platform = json_get_str(in, "platform", "linux");

    json_t *a = json_object();
    json_set_hdr(a, "src", live_root);
    json_set_hdr(a, "dst", previous);
    json_t *rn = md2_call(io, "_rename_riding_out_file_lock", a);
    json_free(a);
    if (!md2_raised(rn)) { json_free(rn); return json_null(); }
    if (strcmp(md2_exc_type(rn), "PermissionError") != 0)
        return md2_propagate(rn);
    if (strcmp(platform, "win32") != 0)
        return md2_propagate(rn);
    /* rn stays alive: the bare `raise` below re-raises THIS exception. */

    json_t *a2 = json_object();
    json_set_hdr(a2, "live_root", live_root);
    json_t *consent = json_obj_get(in, "consent");
    json_set(a2, "consent", consent ? json_copy(consent) : json_null());
    json_t *rr = md2_call(io, "release_after_refused_rename", a2);
    json_free(a2);
    if (md2_raised(rr)) { json_free(rn); return md2_propagate(rr); }
    json_t *holders = json_obj_get(rr, "holders");
    bool closed = md2_truth(json_obj_get(rr, "closed"));
    if (!closed) {
        if (held_by && held_by->type == JSON_ARRAY && holders && holders->type == JSON_ARRAY) {
            for (size_t i = 0; i < holders->c.count; i++)
                json_append(held_by, json_copy(holders->c.items[i]));
        }
        json_free(rr);
        return md2_propagate(rn);
    }
    json_free(rr);

    json_t *a3 = json_object();
    json_set_hdr(a3, "src", live_root);
    json_set_hdr(a3, "dst", previous);
    json_t *rn2 = md2_call(io, "_rename_riding_out_file_lock", a3);
    json_free(a3);
    if (!md2_raised(rn2)) { json_free(rn2); json_free(rn); return json_null(); }
    if (strcmp(md2_exc_type(rn2), "PermissionError") != 0) {
        json_free(rn);
        return md2_propagate(rn2);
    }
    if (held_by && held_by->type == JSON_ARRAY) {
        json_t *a4 = json_object();
        json_set_hdr(a4, "live_root", live_root);
        json_t *fh = md2_call(io, "find_release_cwd_holders", a4);
        json_free(a4);
        if (md2_raised(fh)) { json_free(rn); json_free(rn2); return md2_propagate(fh); }
        if (fh && fh->type == JSON_ARRAY) {
            for (size_t i = 0; i < fh->c.count; i++)
                json_append(held_by, json_copy(fh->c.items[i]));
        }
        json_free(fh);
    }
    json_free(rn);
    return md2_propagate(rn2);
}

/* ── _discard_desktop_staging ──────────────────────────────────────────── */
/* PoP: md2_discard_desktop_staging @ hermes_cli/main_desktop.py:_discard_desktop_staging */
static json_t *md2_discard_desktop_staging(md2_io_t *io, json_t *in) {
    json_t *rm = md2_rmtree(io, json_get_str(in, "staging_dir", ""));
    MD2_PROP(rm);
    json_free(rm);
    return json_null();
}

/* ── _kernel32 ─────────────────────────────────────────────────────────── */
/* PoP: md2_kernel32 @ hermes_cli/main_desktop.py:_kernel32 */
static json_t *md2_kernel32(md2_io_t *io, json_t *in) {
    (void)in;
    json_t *a = json_object();
    json_set_hdr(a, "name", "kernel32");
    json_set(a, "use_last_error", json_bool(true));
    json_t *r = md2_call(io, "ctypes.WinDLL", a);
    json_free(a);
    return r;   /* opaque WinDLL marker (or raise) */
}

/* ── _runs_from ────────────────────────────────────────────────────────── */
/* PoP: md2_runs_from @ hermes_cli/main_desktop.py:_runs_from */
static json_t *md2_runs_from(md2_io_t *io, json_t *in) {
    json_t *proc = json_obj_get(in, "proc");
    const char *pid = json_get_str(proc, "id", "");
    const char *release_dir = json_get_str(in, "release_dir", "");
    json_t *a = json_object();
    json_set_hdr(a, "proc", pid);
    json_t *pe = md2_call(io, "proc.exe", a);
    json_free(a);
    if (md2_raised(pe)) {
        if (md2_exc_is_exception(pe)) { json_free(pe); return json_bool(false); }
        return md2_propagate(pe);
    }
    json_t *ctor = md2_path_ctor(pe);
    json_free(pe);
    if (md2_raised(ctor)) {
        if (md2_exc_is_exception(ctor)) { json_free(ctor); return json_bool(false); }
        return md2_propagate(ctor);
    }
    json_t *rs = md2_path_resolve(io, ctor->str_val);
    json_free(ctor);
    if (md2_raised(rs)) {
        if (md2_exc_is_exception(rs)) { json_free(rs); return json_bool(false); }
        return md2_propagate(rs);
    }
    json_t *parents = md2_pparents(md2_jstr(rs));
    json_free(rs);
    bool has = false;
    for (size_t i = 0; i < json_len(parents); i++) {
        if (strcmp(json_get(parents, i)->str_val, release_dir) == 0) { has = true; break; }
    }
    json_free(parents);
    return json_bool(has);
}

/* ── _desktop_ancestor_in ──────────────────────────────────────────────── */
/* PoP: md2_desktop_ancestor_in @ hermes_cli/main_desktop.py:_desktop_ancestor_in */
static json_t *md2_desktop_ancestor_in(md2_io_t *io, json_t *in) {
    const char *desktop_dir = json_get_str(in, "desktop_dir", "");
    char *rd = md2_pjoin(desktop_dir, "release");
    json_t *rs = md2_path_resolve(io, rd);
    free(rd);
    if (md2_raised(rs)) {
        if (md2_exc_is_exception(rs)) { json_free(rs); return json_null(); }
        return md2_propagate(rs);
    }
    json_t *pidr = md2_getpid(io);
    if (md2_raised(pidr)) {
        if (md2_exc_is_exception(pidr)) { json_free(pidr); json_free(rs); return json_null(); }
        json_free(rs);
        return md2_propagate(pidr);
    }
    json_t *a = json_object();
    json_set(a, "pid", json_copy(pidr));
    json_free(pidr);
    json_t *pr = md2_call(io, "psutil.Process", a);
    json_free(a);
    if (md2_raised(pr)) {
        if (md2_exc_is_exception(pr)) { json_free(pr); json_free(rs); return json_null(); }
        json_free(rs);
        return md2_propagate(pr);
    }
    json_t *a2 = json_object();
    json_set_hdr(a2, "proc", json_get_str(pr, "id", ""));
    json_free(pr);
    json_t *pa = md2_call(io, "proc.parents", a2);
    json_free(a2);
    if (md2_raised(pa)) {
        if (md2_exc_is_exception(pa)) { json_free(pa); json_free(rs); return json_null(); }
        json_free(rs);
        return md2_propagate(pa);
    }
    /* loop OUTSIDE the try: _runs_from never raises Exception kinds. */
    json_t *ret = json_null();
    for (size_t i = 0; i < json_len(pa); i++) {
        json_t *parent = json_get(pa, i);
        json_t *sub_in = json_object();
        json_t *proc_obj = json_object();
        json_set_hdr(proc_obj, "id", json_get_str(parent, "id", ""));
        json_set(sub_in, "proc", proc_obj);
        json_set_hdr(sub_in, "release_dir", md2_jstr(rs));
        json_t *rf = md2_runs_from(io, sub_in);
        json_free(sub_in);
        if (md2_raised(rf)) { json_free(pa); json_free(rs); return md2_propagate(rf); }
        bool runs = md2_truth(rf);
        json_free(rf);
        if (runs) {
            json_free(ret);
            ret = json_number(json_get_num(parent, "pid", 0));
            break;
        }
    }
    json_free(pa);
    json_free(rs);
    return ret;
}

/* ── _sandbox_helper_lstat ─────────────────────────────────────────────── */
/* PoP: md2_sandbox_helper_lstat @ hermes_cli/main_desktop.py:_sandbox_helper_lstat */
static json_t *md2_sandbox_helper_lstat(md2_io_t *io, json_t *in) {
    const char *pe = json_get_str(in, "packaged_executable", "");
    char *par = md2_pparent(pe);
    char *sandbox = md2_pjoin(par, "chrome-sandbox");
    free(par);
    json_t *st = md2_path_pred(io, "path.lstat", sandbox);
    json_t *out = json_array();
    json_append(out, json_string(sandbox));
    free(sandbox);
    if (md2_raised(st)) {
        if (md2_exc_is_a(md2_exc_type(st), "OSError")) {
            json_free(st);
            json_append(out, json_null());
            return out;
        }
        json_free(out);
        return md2_propagate(st);
    }
    json_append(out, st);
    return out;
}

/* ── _stage_macos_bundle_copy ──────────────────────────────────────────── */
/* PoP: md2_stage_macos_bundle_copy @ hermes_cli/main_desktop.py:_stage_macos_bundle_copy */
static json_t *md2_stage_macos_bundle_copy(md2_io_t *io, json_t *in) {
    json_t *argv = json_array();
    json_append(argv, json_string("/usr/bin/ditto"));
    json_append(argv, json_string(json_get_str(in, "src", "")));
    json_append(argv, json_string(json_get_str(in, "dst", "")));
    json_t *kw = json_object();
    json_set(kw, "check", json_bool(true));
    json_t *r = md2_subprocess_run(io, argv, kw);
    json_free(argv);
    json_free(kw);
    MD2_PROP(r);
    json_free(r);
    return json_null();
}

/* ── _launch_installed_macos_desktop_app ───────────────────────────────── */
/* PoP: md2_launch_installed_macos_desktop_app @ hermes_cli/main_desktop.py:_launch_installed_macos_desktop_app */
static json_t *md2_launch_installed_macos_desktop_app(md2_io_t *io, json_t *in) {
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") != 0)
        return json_bool(false);
    const char *executable = "/Applications/Hermes.app/Contents/MacOS/Hermes";
    json_t *isf = md2_path_pred(io, "path.is_file", executable);
    MD2_PROP(isf);
    bool is_file = md2_truth(isf);
    json_free(isf);
    if (!is_file) return json_bool(false);
    char *par = md2_pparent(executable);
    json_t *a = json_object();
    json_t *cmd = json_array();
    json_append(cmd, json_string(executable));
    json_set(a, "launch_command", cmd);
    json_set_hdr(a, "cwd", par);
    free(par);
    json_t *pidr = md2_call(io, "launch_detached", a);
    json_free(a);
    MD2_PROP(pidr);
    md2_sbuf b; md2_sb_init(&b);
    md2_sb_add(&b, MD2_ARROW " Launched the installed Hermes Desktop app: ");
    md2_sb_add(&b, executable);
    md2_sb_add(&b, " (pid ");
    md2_sb_addnum(&b, md2_is_num(pidr) ? pidr->num_val : 0);
    md2_sb_add(&b, ")");
    json_free(pidr);
    char *m = md2_sb_take(&b);
    json_t *pr = md2_print(io, m);
    free(m);
    MD2_PROP(pr);
    json_free(pr);
    return json_bool(true);
}
/* ── _desktop_macos_bundle_id ──────────────────────────────────────────── */
/* PoP: md2_desktop_macos_bundle_id @ hermes_cli/main_desktop.py:_desktop_macos_bundle_id */
static json_t *md2_desktop_macos_bundle_id(md2_io_t *io, json_t *in) {
    const char *bundle = json_get_str(in, "bundle", "");
    char *c1 = md2_pjoin(bundle, "Contents");
    char *info = md2_pjoin(c1, "Info.plist");
    free(c1);
    json_t *ex = md2_path_pred(io, "path.exists", info);
    if (md2_raised(ex)) { free(info); return md2_propagate(ex); }
    bool exists = md2_truth(ex);
    json_free(ex);
    if (!exists) {
        char *suf = md2_psuffix(bundle);
        if (strcmp(suf, ".framework") == 0) {
            json_t *g1 = md2_path_glob(io, bundle, "Versions/*/Resources/Info.plist");
            if (md2_raised(g1)) { free(suf); free(info); return md2_propagate(g1); }
            json_t *g2 = md2_path_glob(io, bundle, "Resources/Info.plist");
            if (md2_raised(g2)) { json_free(g1); free(suf); free(info); return md2_propagate(g2); }
            size_t n1 = json_len(g1), n2 = json_len(g2);
            if (n1 + n2 > 0) {
                const char *first = n1 > 0 ? json_get(g1, 0)->str_val : json_get(g2, 0)->str_val;
                free(info);
                info = md2_dup(first);
            }
            json_free(g1);
            json_free(g2);
        }
        free(suf);
    }
    json_t *ex2 = md2_path_pred(io, "path.exists", info);
    if (md2_raised(ex2)) { free(info); return md2_propagate(ex2); }
    bool exists2 = md2_truth(ex2);
    json_free(ex2);
    if (!exists2) { free(info); return json_null(); }
    /* try: data = plistlib.loads(info.read_bytes()) except Exception: None */
    json_t *rb = md2_path_read_bytes(io, info);
    free(info);
    if (md2_raised(rb)) {
        if (md2_exc_is_exception(rb)) { json_free(rb); return json_null(); }
        return md2_propagate(rb);
    }
    json_t *a = json_object();
    json_set_hdr(a, "data", md2_jstr(rb));
    json_free(rb);
    json_t *data = md2_call(io, "plistlib.loads", a);
    json_free(a);
    if (md2_raised(data)) {
        if (md2_exc_is_exception(data)) { json_free(data); return json_null(); }
        return md2_propagate(data);
    }
    json_t *ident = json_obj_get(data, "CFBundleIdentifier");
    json_t *out = json_null();
    if (md2_truth(ident)) {
        char *s = md2_pystr(ident);
        json_free(out);
        out = json_string(s);
        free(s);
    }
    json_free(data);
    return out;
}

/* ── _desktop_macos_local_signing_identity ─────────────────────────────── */
/* PoP: md2_desktop_macos_local_signing_identity @ hermes_cli/main_desktop.py:_desktop_macos_local_signing_identity */
static json_t *md2_desktop_macos_local_signing_identity(md2_io_t *io, json_t *in) {
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") != 0)
        return json_null();
    json_t *a = json_object();
    json_t *lc = md2_call(io, "load_config", a);
    json_free(a);
    char *warn_exc = NULL;
    if (md2_raised(lc)) {
        if (!md2_exc_is_exception(lc)) return md2_propagate(lc);
        warn_exc = md2_exc_str(lc);
        json_free(lc);
        lc = NULL;
    } else if (!md2_is_obj(lc)) {
        /* load_config().get(...) on a non-dict raises AttributeError */
        const char *tn = md2_is_none(lc) ? "NoneType"
                       : md2_is_arr(lc) ? "list"
                       : md2_is_str(lc) ? "str"
                       : md2_is_num(lc) ? "int"
                       : md2_is_bool(lc) ? "bool" : "object";
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "'");
        md2_sb_add(&b, tn);
        md2_sb_add(&b, "' object has no attribute 'get'");
        warn_exc = md2_sb_take(&b);
        json_free(lc);
        lc = NULL;
    }
    if (warn_exc) {
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "  (warning: could not load desktop.macos_signing_identity: ");
        md2_sb_add(&b, warn_exc);
        md2_sb_add(&b, "; falling back to ad-hoc signing)");
        free(warn_exc);
        char *m = md2_sb_take(&b);
        json_t *pr = md2_print(io, m);
        free(m);
        MD2_PROP(pr);
        json_free(pr);
        return json_null();
    }
    json_t *desktop = json_obj_get(lc, "desktop");   /* absent -> {} (dict) */
    json_t *out = json_null();
    if (md2_is_obj(desktop)) {
        json_t *identity = json_obj_get(desktop, "macos_signing_identity");
        if (md2_is_str(identity)) {
            char *st = md2_strip(identity->str_val);
            if (st[0]) {
                json_free(out);
                out = json_string(st);
            }
            free(st);
        }
    }
    json_free(lc);
    return out;
}

/* ── _codesign_verify ──────────────────────────────────────────────────── */
/* PoP: md2_codesign_verify @ hermes_cli/main_desktop.py:_codesign_verify */
static json_t *md2_codesign_verify(md2_io_t *io, json_t *in) {
    json_t *argv = json_array();
    json_append(argv, json_string(json_get_str(in, "codesign", "")));
    json_append(argv, json_string("--verify"));
    json_append(argv, json_string("--deep"));
    json_append(argv, json_string("--strict"));
    json_append(argv, json_string(json_get_str(in, "app", "")));
    json_t *r = md2_subprocess_run(io, argv, json_obj_get(in, "kwargs"));
    json_free(argv);
    return r;   /* CompletedProcess projection {returncode,stdout,stderr} or raise */
}

/* field(key): first output line starting "key=" -> stripped value or None. */
static char *md2_sig_field(json_t *lines, const char *key) {
    md2_sbuf pre; md2_sb_init(&pre);
    md2_sb_add(&pre, key);
    md2_sb_add(&pre, "=");
    char *prefix = md2_sb_take(&pre);
    size_t pn = strlen(prefix);
    char *out = NULL;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = json_get(lines, i)->str_val;
        if (md2_startswith(line, prefix)) {
            char *st = md2_strip(line + pn);
            if (st[0]) out = st;
            else free(st);
            break;
        }
    }
    free(prefix);
    return out;
}

/* ── _macos_signature_summary ──────────────────────────────────────────── */
/* PoP: md2_macos_signature_summary @ hermes_cli/main_desktop.py:_macos_signature_summary */
static json_t *md2_macos_signature_summary(md2_io_t *io, json_t *in) {
    const char *codesign = json_get_str(in, "codesign", "");
    const char *app = json_get_str(in, "app", "");
    json_t *argv = json_array();
    json_append(argv, json_string(codesign));
    json_append(argv, json_string("-dv"));
    json_append(argv, json_string(app));
    json_t *kw = json_object();
    json_set(kw, "check", json_bool(false));
    json_set(kw, "text", json_bool(true));
    json_set_hdr(kw, "encoding", "utf-8");
    json_set_hdr(kw, "errors", "replace");
    json_t *info = md2_subprocess_run(io, argv, kw);
    json_free(argv);
    json_free(kw);
    if (md2_raised(info)) {
        if (md2_exc_is_exception(info)) { json_free(info); return json_null(); }
        return md2_propagate(info);
    }
    char *so = md2_pystr(json_obj_get(info, "stdout"));
    char *se = md2_pystr(json_obj_get(info, "stderr"));
    md2_sbuf b; md2_sb_init(&b);
    md2_sb_add(&b, so);
    md2_sb_add(&b, "\n");
    md2_sb_add(&b, se);
    free(so);
    free(se);
    char *output = md2_sb_take(&b);
    double rc = json_get_num(info, "returncode", 0);
    json_free(info);
    if (rc != 0) { free(output); return json_null(); }
    json_t *lines = md2_splitlines(output);
    free(output);
    char *team = md2_sig_field(lines, "TeamIdentifier");
    char *identifier = md2_sig_field(lines, "Identifier");
    json_free(lines);
    /* verified: _codesign_verify(codesign, app, check=False).returncode == 0
     * (OUTSIDE the try — a raise here propagates). */
    json_t *sub_in = json_object();
    json_set_hdr(sub_in, "codesign", codesign);
    json_set_hdr(sub_in, "app", app);
    json_t *vkw = json_object();
    json_set(vkw, "check", json_bool(false));
    json_set(sub_in, "kwargs", vkw);
    json_t *cv = md2_codesign_verify(io, sub_in);
    json_free(sub_in);
    if (md2_raised(cv)) {
        free(team);
        free(identifier);
        return md2_propagate(cv);
    }
    bool verified = json_get_num(cv, "returncode", 0) == 0;
    json_free(cv);
    json_t *out = json_object();
    if (team && strcmp(team, "not set") != 0) json_set_hdr(out, "team", team);
    else json_set(out, "team", json_null());
    if (identifier) json_set_hdr(out, "identifier", identifier);
    else json_set(out, "identifier", json_null());
    json_set(out, "verified", json_bool(verified));
    free(team);
    free(identifier);
    return out;
}

/* ── _desktop_macos_has_valid_real_signature ───────────────────────────── */
/* PoP: md2_desktop_macos_has_valid_real_signature @ hermes_cli/main_desktop.py:_desktop_macos_has_valid_real_signature */
static json_t *md2_desktop_macos_has_valid_real_signature(md2_io_t *io, json_t *in) {
    const char *app = json_get_str(in, "app", "");
    json_t *w = md2_which(io, "codesign");
    MD2_PROP(w);
    if (!md2_truth(w)) { json_free(w); return json_bool(false); }
    const char *codesign = md2_jstr(w);
    json_t *argv = json_array();
    json_append(argv, json_string(codesign));
    json_append(argv, json_string("-dv"));
    json_append(argv, json_string(app));
    json_t *kw = json_object();
    json_set(kw, "check", json_bool(false));
    json_set(kw, "text", json_bool(true));
    json_set_hdr(kw, "encoding", "utf-8");
    json_set_hdr(kw, "errors", "replace");
    json_t *info = md2_subprocess_run(io, argv, kw);
    json_free(argv);
    json_free(kw);
    if (md2_raised(info)) {
        if (md2_exc_is_exception(info)) { json_free(info); json_free(w); return json_bool(false); }
        json_free(w);
        return md2_propagate(info);
    }
    char *so = md2_pystr(json_obj_get(info, "stdout"));
    char *se = md2_pystr(json_obj_get(info, "stderr"));
    md2_sbuf b; md2_sb_init(&b);
    md2_sb_add(&b, so);
    md2_sb_add(&b, "\n");
    md2_sb_add(&b, se);
    free(so);
    free(se);
    char *output = md2_sb_take(&b);
    double rc = json_get_num(info, "returncode", 0);
    json_free(info);
    bool bad = rc != 0 || !strstr(output, "TeamIdentifier=")
            || strstr(output, "TeamIdentifier=not set") != NULL;
    free(output);
    if (bad) { json_free(w); return json_bool(false); }
    json_t *sub_in = json_object();
    json_set_hdr(sub_in, "codesign", codesign);
    json_set_hdr(sub_in, "app", app);
    json_t *vkw = json_object();
    json_set(vkw, "check", json_bool(false));
    json_set(sub_in, "kwargs", vkw);
    json_free(w);
    json_t *cv = md2_codesign_verify(io, sub_in);
    json_free(sub_in);
    if (md2_raised(cv)) {
        if (md2_exc_is_exception(cv)) { json_free(cv); return json_bool(false); }
        return md2_propagate(cv);
    }
    bool ok = json_get_num(cv, "returncode", 0) == 0;
    json_free(cv);
    return json_bool(ok);
}

/* ── _macos_legacy_adhoc_resign ────────────────────────────────────────── */
/* PoP: md2_macos_legacy_adhoc_resign @ hermes_cli/main_desktop.py:_macos_legacy_adhoc_resign */
static json_t *md2_macos_legacy_adhoc_resign(md2_io_t *io, json_t *in) {
    const char *codesign = json_get_str(in, "codesign", "");
    const char *app = json_get_str(in, "app", "");
    json_t *argv = json_array();
    json_append(argv, json_string(codesign));
    json_append(argv, json_string("--force"));
    json_append(argv, json_string("--deep"));
    json_append(argv, json_string("--sign"));
    json_append(argv, json_string("-"));
    json_append(argv, json_string(app));
    json_t *kw = json_object();
    json_set(kw, "check", json_bool(false));
    json_set(kw, "text", json_bool(true));
    json_set_hdr(kw, "encoding", "utf-8");
    json_set_hdr(kw, "errors", "replace");
    json_t *result = md2_subprocess_run(io, argv, kw);
    json_free(argv);
    json_free(kw);
    char *skip_exc = NULL;
    if (md2_raised(result)) {
        if (!md2_exc_is_exception(result)) return md2_propagate(result);
        skip_exc = md2_exc_str(result);
        json_free(result);
        result = NULL;
    }
    if (!skip_exc) {
        double rc = json_get_num(result, "returncode", 0);
        json_free(result);
        if (rc != 0) {
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "  (warning: legacy ad-hoc re-sign failed (exit ");
            md2_sb_addnum(&b, rc);
            md2_sb_add(&b, "); leaving safeStorage keychain item untouched)");
            char *m = md2_sb_take(&b);
            json_t *pr = md2_print(io, m);
            free(m);
            MD2_PROP(pr);
            json_free(pr);
            return json_bool(false);
        }
        json_t *sub_in = json_object();
        json_set_hdr(sub_in, "codesign", codesign);
        json_set_hdr(sub_in, "app", app);
        json_t *vkw = json_object();
        json_set(vkw, "check", json_bool(false));
        json_set(vkw, "text", json_bool(true));
        json_set_hdr(vkw, "encoding", "utf-8");
        json_set_hdr(vkw, "errors", "replace");
        json_set(sub_in, "kwargs", vkw);
        json_t *cv = md2_codesign_verify(io, sub_in);
        json_free(sub_in);
        if (md2_raised(cv)) {
            if (!md2_exc_is_exception(cv)) return md2_propagate(cv);
            skip_exc = md2_exc_str(cv);
            json_free(cv);
        } else {
            double vrc = json_get_num(cv, "returncode", 0);
            json_free(cv);
            if (vrc != 0) {
                json_t *pr = md2_print(io,
                    "  (warning: legacy ad-hoc re-sign did not pass strict verification; "
                    "leaving safeStorage keychain item untouched)");
                MD2_PROP(pr);
                json_free(pr);
                return json_bool(false);
            }
            json_t *pr = md2_print(io,
                "  " MD2_ARROW " macOS desktop re-signed (legacy ad-hoc); "
                "safeStorage keychain item left untouched");
            MD2_PROP(pr);
            json_free(pr);
            return json_bool(true);
        }
    }
    md2_sbuf b; md2_sb_init(&b);
    md2_sb_add(&b, "  (warning: macOS relaunch fixup skipped: ");
    md2_sb_add(&b, skip_exc);
    md2_sb_add(&b, ")");
    free(skip_exc);
    char *m = md2_sb_take(&b);
    json_t *pr = md2_print(io, m);
    free(m);
    MD2_PROP(pr);
    json_free(pr);
    return json_bool(false);
}

/* ── _app_asar_hash ────────────────────────────────────────────────────── */
/* PoP: md2_app_asar_hash @ hermes_cli/main_desktop.py:_app_asar_hash */
static json_t *md2_app_asar_hash(md2_io_t *io, json_t *in) {
    const char *app_path = json_get_str(in, "app_path", "");
    char *c1 = md2_pjoin(app_path, "Contents");
    char *c2 = md2_pjoin(c1, "Resources");
    char *asar = md2_pjoin(c2, "app.asar");
    free(c1);
    free(c2);
    json_t *isf = md2_path_pred(io, "path.is_file", asar);
    if (md2_raised(isf)) { free(asar); return md2_propagate(isf); }
    bool is_file = md2_truth(isf);
    json_free(isf);
    if (!is_file) { free(asar); return json_null(); }
    json_t *op = md2_open_rb(io, asar);
    if (md2_raised(op)) {
        if (md2_exc_is_a(md2_exc_type(op), "OSError")) { json_free(op); free(asar); return json_null(); }
        free(asar);
        return md2_propagate(op);
    }
    json_free(op);
    md2_sha256_ctx h;
    md2_sha256_init(&h);
    json_t *ret = NULL;
    for (;;) {
        json_t *fr = md2_file_read(io, asar, 65536);
        if (md2_raised(fr)) {
            if (md2_exc_is_a(md2_exc_type(fr), "OSError")) {
                json_free(fr);
                ret = json_null();
                break;
            }
            free(asar);
            return md2_propagate(fr);
        }
        size_t n = 0;
        unsigned char *bytes = md2_hex_decode(md2_jstr(fr), &n);
        json_free(fr);
        if (n == 0) { free(bytes); break; }
        md2_sha256_update(&h, bytes, n);
        free(bytes);
    }
    free(asar);
    if (ret) return ret;
    char *dig = md2_sha256_final(&h);
    json_t *out = json_string(dig);
    free(dig);
    return out;
}

/* ── _swap_in_new_macos_bundle ─────────────────────────────────────────── */
/* PoP: md2_swap_in_new_macos_bundle @ hermes_cli/main_desktop.py:_swap_in_new_macos_bundle */
static json_t *md2_swap_in_new_macos_bundle(md2_io_t *io, json_t *in) {
    const char *tmp = json_get_str(in, "tmp", "");
    const char *target = json_get_str(in, "target", "");
    const char *old = json_get_str(in, "old", "");
    bool moved_old = false;
    json_t *ex = md2_path_pred(io, "path.exists", target);
    MD2_PROP(ex);
    bool target_exists = md2_truth(ex);
    json_free(ex);
    if (target_exists) {
        json_t *rn = md2_path_rename(io, target, old);
        if (md2_raised(rn)) {
            if (md2_exc_is_a(md2_exc_type(rn), "OSError")) {
                json_t *rm = md2_rmtree(io, tmp);
                if (md2_raised(rm)) { json_free(rn); return md2_propagate(rm); }
                json_free(rm);
                return md2_propagate(rn);
            }
            return md2_propagate(rn);
        }
        json_free(rn);
        moved_old = true;
    }
    json_t *rn2 = md2_path_rename(io, tmp, target);
    if (md2_raised(rn2)) {
        if (!md2_exc_is_a(md2_exc_type(rn2), "OSError"))
            return md2_propagate(rn2);
        json_t *rollback_error = NULL;
        if (moved_old) {
            json_t *rb = md2_path_rename(io, old, target);
            if (md2_raised(rb)) {
                if (md2_exc_is_a(md2_exc_type(rb), "OSError")) rollback_error = rb;
                else { json_free(rn2); return md2_propagate(rb); }
            } else json_free(rb);
        }
        json_t *rm = md2_rmtree(io, tmp);
        if (md2_raised(rm)) {
            json_free(rn2);
            if (rollback_error) json_free(rollback_error);
            return md2_propagate(rm);
        }
        json_free(rm);
        if (rollback_error) {
            char *es = md2_exc_str(rollback_error);
            json_free(rollback_error);
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "installing the staged bundle failed and rollback remains at ");
            md2_sb_add(&b, old);
            md2_sb_add(&b, ": ");
            md2_sb_add(&b, es);
            free(es);
            char *m = md2_sb_take(&b);
            json_free(rn2);
            json_t *out = md2_raise_desc("OSError", m);
            free(m);
            return out;
        }
        return md2_propagate(rn2);
    }
    json_free(rn2);
    json_t *rm = md2_rmtree(io, old);
    MD2_PROP(rm);
    json_free(rm);
    return json_null();
}

/* ── _running_macos_app_bundles ────────────────────────────────────────── */
static int md2_cmp_strp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* PoP: md2_running_macos_app_bundles @ hermes_cli/main_desktop.py:_running_macos_app_bundles */
static json_t *md2_running_macos_app_bundles(md2_io_t *io, json_t *in) {
    (void)in;
    json_t *a = json_object();
    json_t *attrs = json_array();
    json_append(attrs, json_string("exe"));
    json_set(a, "attrs", attrs);
    json_t *pi = md2_call(io, "psutil.process_iter", a);
    json_free(a);
    MD2_PROP(pi);
    json_t *bundles = json_array();
    for (size_t i = 0; i < json_len(pi); i++) {
        json_t *rec = json_get(pi, i);
        const char *exe = json_get_str(rec, "exe", "");
        if (!exe) exe = "";
        if (!md2_endswith(exe, "/Contents/MacOS/Hermes")) continue;
        json_t *rs = md2_path_resolve(io, exe);
        if (md2_raised(rs)) { json_free(pi); json_free(bundles); return md2_propagate(rs); }
        json_t *parents = md2_pparents(md2_jstr(rs));
        json_free(rs);
        if (json_len(parents) < 3) {
            json_free(parents);
            json_free(pi);
            json_free(bundles);
            return md2_raise_desc("IndexError", "2");
        }
        const char *bundle = json_get(parents, 2)->str_val;
        bool dup = false;
        for (size_t k = 0; k < json_len(bundles); k++)
            if (strcmp(json_get(bundles, k)->str_val, bundle) == 0) { dup = true; break; }
        if (!dup) json_append(bundles, json_string(bundle));
        json_free(parents);
    }
    json_free(pi);
    /* set -> sorted ascending array */
    size_t n = json_len(bundles);
    if (n > 1) {
        const char **arr = malloc(n * sizeof(char *));
        for (size_t i = 0; i < n; i++) arr[i] = json_get(bundles, i)->str_val;
        qsort(arr, n, sizeof(char *), md2_cmp_strp);
        json_t *sorted = json_array();
        for (size_t i = 0; i < n; i++) json_append(sorted, json_string(arr[i]));
        free(arr);
        json_free(bundles);
        bundles = sorted;
    }
    return bundles;
}

/* ── _update_owned_macos_bundles ───────────────────────────────────────── */
/* PoP: md2_update_owned_macos_bundles @ hermes_cli/main_desktop.py:_update_owned_macos_bundles */
static json_t *md2_update_owned_macos_bundles(md2_io_t *io, json_t *in) {
    json_t *candidates = json_obj_get(in, "candidates");
    json_t *owned = json_array();
    for (size_t i = 0; i < json_len(candidates); i++) {
        const char *app = json_get(candidates, i)->str_val;
        char *c1 = md2_pjoin(app, "Contents");
        char *c2 = md2_pjoin(c1, "Resources");
        char *p = md2_pjoin(c2, "install-stamp.json");
        free(c1);
        free(c2);
        json_t *rt = md2_path_read_text(io, p, "utf-8-sig");
        free(p);
        if (md2_raised(rt)) {
            const char *t = md2_exc_type(rt);
            if (md2_exc_is_a(t, "OSError") || md2_exc_is_a(t, "ValueError")) {
                json_free(rt);
                continue;
            }
            json_free(owned);
            return md2_propagate(rt);
        }
        json_t *stamp = md2_json_loads(md2_jstr(rt));
        json_free(rt);
        if (md2_raised(stamp)) {
            const char *t = md2_exc_type(stamp);
            if (md2_exc_is_a(t, "OSError") || md2_exc_is_a(t, "ValueError")) {
                json_free(stamp);
                continue;
            }
            json_free(owned);
            return md2_propagate(stamp);
        }
        bool is_owned = false;
        if (md2_is_obj(stamp)) {
            json_t *um = json_obj_get(stamp, "updateMechanism");
            if (!um) is_owned = true;   /* .get("updateMechanism", "self") */
            else if (md2_is_str(um) && strcmp(um->str_val, "self") == 0) is_owned = true;
        }
        json_free(stamp);
        if (is_owned) json_append(owned, json_string(app));
    }
    return owned;
}

/* ── _owns_installed_desktop_apps ──────────────────────────────────────── */
/* PoP: md2_owns_installed_desktop_apps @ hermes_cli/main_desktop.py:_owns_installed_desktop_apps */
static json_t *md2_owns_installed_desktop_apps(md2_io_t *io, json_t *in) {
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") != 0)
        return json_bool(false);
    json_t *rs1 = md2_path_resolve(io, json_get_str(in, "project_root", "/repo"));
    MD2_PROP(rs1);
    json_t *a = json_object();
    json_t *root = md2_call(io, "get_default_hermes_root", a);
    json_free(a);
    if (md2_raised(root)) { json_free(rs1); return md2_propagate(root); }
    char *j = md2_pjoin(md2_jstr(root), "hermes-agent");
    json_free(root);
    json_t *rs2 = md2_path_resolve(io, j);
    free(j);
    if (md2_raised(rs2)) { json_free(rs1); return md2_propagate(rs2); }
    bool eq = strcmp(md2_jstr(rs1), md2_jstr(rs2)) == 0;
    json_free(rs1);
    json_free(rs2);
    return json_bool(eq);
}
/* ── _installed_desktop_apps ───────────────────────────────────────────── */
/* PoP: md2_installed_desktop_apps @ hermes_cli/main_desktop.py:_installed_desktop_apps */
static json_t *md2_installed_desktop_apps(md2_io_t *io, json_t *in) {
    json_t *own = md2_owns_installed_desktop_apps(io, in);
    MD2_PROP(own);
    bool owns = md2_truth(own);
    json_free(own);
    if (!owns) return json_array();
    json_t *a = json_object();
    json_t *cand = md2_call(io, "packaged_gui_app_paths", a);
    json_free(a);
    MD2_PROP(cand);
    json_t *sub_in = json_object();
    json_set(sub_in, "candidates", json_copy(cand));
    json_t *owned = md2_update_owned_macos_bundles(io, sub_in);
    json_free(sub_in);
    if (md2_raised(owned)) { json_free(cand); return md2_propagate(owned); }
    if (json_len(owned) > 0) { json_free(cand); return owned; }
    json_free(owned);
    json_t *a2 = json_object();
    json_t *rec_path = md2_call(io, "desktop_install_record", a2);
    json_free(a2);
    if (md2_raised(rec_path)) { json_free(cand); return md2_propagate(rec_path); }
    json_t *a3 = json_object();
    json_set_hdr(a3, "path", md2_jstr(rec_path));
    json_free(rec_path);
    json_t *rj = md2_call(io, "read_json_or_empty", a3);
    json_free(a3);
    if (md2_raised(rj)) { json_free(cand); return md2_propagate(rj); }
    if (!md2_is_obj(rj)) {
        const char *tn = md2_is_none(rj) ? "NoneType" : md2_is_arr(rj) ? "list" : "object";
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "'");
        md2_sb_add(&b, tn);
        md2_sb_add(&b, "' object has no attribute 'get'");
        char *m = md2_sb_take(&b);
        json_free(rj);
        json_free(cand);
        json_t *e = md2_raise_desc("AttributeError", m);
        free(m);
        return e;
    }
    json_t *recorded = json_obj_get(rj, "apps");
    json_t *out = json_array();
    if (md2_is_arr(recorded)) {
        for (size_t i = 0; i < json_len(cand); i++) {
            const char *app = json_get(cand, i)->str_val;
            bool in_rec = false;
            for (size_t k = 0; k < json_len(recorded); k++) {
                json_t *rv = json_get(recorded, k);
                if (md2_is_str(rv) && strcmp(rv->str_val, app) == 0) { in_rec = true; break; }
            }
            if (!in_rec) continue;
            json_t *ex = md2_path_pred(io, "path.exists", app);
            if (md2_raised(ex)) { json_free(out); json_free(rj); json_free(cand); return md2_propagate(ex); }
            bool exists = md2_truth(ex);
            json_free(ex);
            if (!exists) json_append(out, json_string(app));
        }
    }
    json_free(rj);
    json_free(cand);
    return out;
}

/* ── _install_rebuilt_macos_bundles ────────────────────────────────────── */
/* PoP: md2_install_rebuilt_macos_bundles @ hermes_cli/main_desktop.py:_install_rebuilt_macos_bundles */
static json_t *md2_install_rebuilt_macos_bundles(md2_io_t *io, json_t *in) {
    const char *rebuilt_app = json_get_str(in, "rebuilt_app", "");
    json_t *candidates = json_obj_get(in, "candidates");
    json_t *running = json_obj_get(in, "running");
    json_t *empty_pair = NULL;
    json_t *hash_in = json_object();
    json_set_hdr(hash_in, "app_path", rebuilt_app);
    json_t *rh = md2_app_asar_hash(io, hash_in);
    json_free(hash_in);
    MD2_PROP(rh);
    if (md2_is_none(rh)) {
        json_free(rh);
        empty_pair = json_array();
        json_append(empty_pair, json_array());
        json_append(empty_pair, json_array());
        return empty_pair;
    }
    json_t *w = md2_which(io, "codesign");
    if (md2_raised(w)) { json_free(rh); return md2_propagate(w); }
    const char *codesign = md2_truth(w) ? md2_jstr(w) : NULL;
    json_t *rebuilt_sig = json_null();
    if (codesign) {
        json_t *sig_in = json_object();
        json_set_hdr(sig_in, "codesign", codesign);
        json_set_hdr(sig_in, "app", rebuilt_app);
        json_free(rebuilt_sig);
        rebuilt_sig = md2_macos_signature_summary(io, sig_in);
        json_free(sig_in);
        if (md2_raised(rebuilt_sig)) { json_free(w); json_free(rh); return md2_propagate(rebuilt_sig); }
    }
    json_t *installed = json_array();
    json_t *problems = json_array();
    for (size_t i = 0; i < json_len(candidates); i++) {
        const char *app = json_get(candidates, i)->str_val;
        char *par = md2_pparent(app);
        char *name = md2_pname(app);
        md2_sbuf sb; md2_sb_init(&sb);
        md2_sb_add(&sb, name);
        md2_sb_add(&sb, ".hermes-update-old");
        char *aside1 = md2_pjoin(par, md2_sb_take(&sb));
        md2_sb_init(&sb);
        md2_sb_add(&sb, name);
        md2_sb_add(&sb, ".old");
        char *aside2 = md2_pjoin(par, md2_sb_take(&sb));
        md2_sb_init(&sb);
        md2_sb_add(&sb, name);
        md2_sb_add(&sb, ".hermes-update-new");
        char *tmp = md2_pjoin(par, md2_sb_take(&sb));
        md2_sb_init(&sb);
        md2_sb_add(&sb, name);
        md2_sb_add(&sb, ".hermes-update-old");
        char *old = md2_pjoin(par, md2_sb_take(&sb));
        free(name);

        json_t *rec_in = json_object();
        json_set_hdr(rec_in, "live", app);
        json_set_hdr(rec_in, "aside", aside1);
        json_t *r1 = md2_recover_interrupted_swap(io, rec_in);
        json_free(rec_in);
        if (md2_raised(r1)) goto fail_free;
        json_free(r1);
        rec_in = json_object();
        json_set_hdr(rec_in, "live", app);
        json_set_hdr(rec_in, "aside", aside2);
        json_t *r2 = md2_recover_interrupted_swap(io, rec_in);
        json_free(rec_in);
        if (md2_raised(r2)) { r1 = r2; goto fail_free; }
        json_free(r2);

        json_t *isd = md2_path_pred(io, "path.is_dir", app);
        if (md2_raised(isd)) { r1 = isd; goto fail_free; }
        bool is_dir = md2_truth(isd);
        json_free(isd);
        bool current = false;
        if (is_dir) {
            json_t *hin = json_object();
            json_set_hdr(hin, "app_path", app);
            json_t *h = md2_app_asar_hash(io, hin);
            json_free(hin);
            if (md2_raised(h)) { r1 = h; goto fail_free; }
            current = md2_is_str(h) && md2_is_str(rh)
                   && strcmp(h->str_val, rh->str_val) == 0;
            json_free(h);
        }
        if (current) goto next_app;

        json_t *rs = md2_path_resolve(io, app);
        if (md2_raised(rs)) { r1 = rs; goto fail_free; }
        bool in_running = false;
        for (size_t k = 0; k < json_len(running); k++) {
            json_t *rv = json_get(running, k);
            if (md2_is_str(rv) && strcmp(rv->str_val, md2_jstr(rs)) == 0) { in_running = true; break; }
        }
        json_free(rs);
        if (in_running) {
            md2_sb_init(&sb);
            md2_sb_add(&sb, app);
            md2_sb_add(&sb, " is running and was not refreshed; quit Hermes Desktop and run "
                            "`hermes update` again (or update from inside the app)");
            json_append(problems, json_string(md2_sb_take(&sb)));
            goto next_app;
        }
        if (codesign) {
            json_t *sig_in = json_object();
            json_set_hdr(sig_in, "codesign", codesign);
            json_set_hdr(sig_in, "app", app);
            json_t *isig = md2_macos_signature_summary(io, sig_in);
            json_free(sig_in);
            if (md2_raised(isig)) { r1 = isig; goto fail_free; }
            if (!md2_is_none(isig)) {
                json_t *dg_a = json_object();
                json_set(dg_a, "installed", json_copy(isig));
                json_set(dg_a, "rebuilt", json_copy(rebuilt_sig));
                json_t *dg = md2_call(io, "_macos_signing_downgrade_error", dg_a);
                json_free(dg_a);
                if (md2_raised(dg)) { json_free(isig); r1 = dg; goto fail_free; }
                bool refuse = md2_truth(dg);
                if (refuse) {
                    md2_sb_init(&sb);
                    md2_sb_add(&sb, app);
                    md2_sb_add(&sb, " not refreshed: ");
                    md2_sb_add(&sb, md2_jstr(dg));
                    json_append(problems, json_string(md2_sb_take(&sb)));
                }
                json_free(dg);
                json_free(isig);
                if (refuse) goto next_app;
            } else json_free(isig);
        }
        {
            json_t *rm = md2_rmtree(io, tmp);
            if (md2_raised(rm)) { r1 = rm; goto fail_free; }
            json_free(rm);
            json_t *stage_in = json_object();
            json_set_hdr(stage_in, "src", rebuilt_app);
            json_set_hdr(stage_in, "dst", tmp);
            json_t *stage = md2_stage_macos_bundle_copy(io, stage_in);
            json_free(stage_in);
            json_t *err = NULL;
            if (md2_raised(stage)) {
                const char *t = md2_exc_type(stage);
                if (md2_exc_is_a(t, "OSError") || md2_exc_is_a(t, "CalledProcessError")) err = stage;
                else { r1 = stage; goto fail_free; }
            } else json_free(stage);
            if (!err) {
                json_t *swap_in = json_object();
                json_set_hdr(swap_in, "tmp", tmp);
                json_set_hdr(swap_in, "target", app);
                json_set_hdr(swap_in, "old", old);
                json_t *swap = md2_swap_in_new_macos_bundle(io, swap_in);
                json_free(swap_in);
                if (md2_raised(swap)) {
                    const char *t = md2_exc_type(swap);
                    if (md2_exc_is_a(t, "OSError") || md2_exc_is_a(t, "CalledProcessError")) err = swap;
                    else { r1 = swap; goto fail_free; }
                } else json_free(swap);
            }
            if (err) {
                json_t *rm2 = md2_rmtree(io, tmp);
                if (md2_raised(rm2)) { json_free(err); r1 = rm2; goto fail_free; }
                json_free(rm2);
                json_t *ex = md2_path_pred(io, "path.exists", app);
                if (md2_raised(ex)) { json_free(err); r1 = ex; goto fail_free; }
                bool kept = md2_truth(ex);
                json_free(ex);
                char *es = md2_exc_str(err);
                json_free(err);
                md2_sb_init(&sb);
                md2_sb_add(&sb, app);
                md2_sb_add(&sb, " could not be installed (");
                md2_sb_add(&sb, es);
                md2_sb_add(&sb, ")");
                if (kept) md2_sb_add(&sb, "; the previous app was kept");
                free(es);
                json_append(problems, json_string(md2_sb_take(&sb)));
                goto next_app;
            }
        }
        json_append(installed, json_string(app));
    next_app:
        free(par);
        free(aside1);
        free(aside2);
        free(tmp);
        free(old);
        continue;
    fail_free:
        free(par);
        free(aside1);
        free(aside2);
        free(tmp);
        free(old);
        json_free(installed);
        json_free(problems);
        json_free(w);
        json_free(rh);
        json_free(rebuilt_sig);
        return md2_propagate(r1);
    }
    json_free(w);
    json_free(rh);
    json_free(rebuilt_sig);
    json_t *out = json_array();
    json_append(out, installed);
    json_append(out, problems);
    return out;
}

/* ── _install_rebuilt_desktop_app ──────────────────────────────────────── */
/* PoP: md2_install_rebuilt_desktop_app @ hermes_cli/main_desktop.py:_install_rebuilt_desktop_app */
static json_t *md2_install_rebuilt_desktop_app(md2_io_t *io, json_t *in) {
    json_t *empty_pair = NULL;
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") != 0) {
        empty_pair = json_array();
        json_append(empty_pair, json_array());
        json_append(empty_pair, json_array());
        return empty_pair;
    }
    json_t *a = json_object();
    json_set_hdr(a, "desktop_dir", json_get_str(in, "desktop_dir", ""));
    json_t *exe = md2_call(io, "_desktop_packaged_executable", a);
    json_free(a);
    MD2_PROP(exe);
    if (md2_is_none(exe)) {
        json_free(exe);
        empty_pair = json_array();
        json_append(empty_pair, json_array());
        json_append(empty_pair, json_array());
        return empty_pair;
    }
    json_t *parents = md2_pparents(md2_jstr(exe));
    json_free(exe);
    if (json_len(parents) < 3) {
        json_free(parents);
        return md2_raise_desc("IndexError", "2");
    }
    json_t *run = md2_running_macos_app_bundles(io, in);
    if (md2_raised(run)) { json_free(parents); return md2_propagate(run); }
    json_t *sub_in = json_object();
    json_set_hdr(sub_in, "rebuilt_app", json_get(parents, 2)->str_val);
    json_set(sub_in, "candidates", json_copy(json_obj_get(in, "candidates")));
    json_set(sub_in, "running", run);
    json_free(parents);
    json_t *r = md2_install_rebuilt_macos_bundles(io, sub_in);
    json_free(sub_in);
    return r;
}

/* ── _refresh_installed_desktop_apps ───────────────────────────────────── */
/* PoP: md2_refresh_installed_desktop_apps @ hermes_cli/main_desktop.py:_refresh_installed_desktop_apps */
static json_t *md2_refresh_installed_desktop_apps(md2_io_t *io, json_t *in) {
    json_t *own = md2_owns_installed_desktop_apps(io, in);
    MD2_PROP(own);
    bool owns = md2_truth(own);
    json_free(own);
    if (!owns) return json_null();
    json_t *owned = md2_installed_desktop_apps(io, in);
    MD2_PROP(owned);
    json_t *missing = json_array();
    for (size_t i = 0; i < json_len(owned); i++) {
        const char *app = json_get(owned, i)->str_val;
        json_t *ex = md2_path_pred(io, "path.exists", app);
        if (md2_raised(ex)) { json_free(missing); json_free(owned); return md2_propagate(ex); }
        bool exists = md2_truth(ex);
        json_free(ex);
        if (!exists) json_append(missing, json_string(app));
    }
    json_t *sub_in = json_object();
    json_set_hdr(sub_in, "desktop_dir", json_get_str(in, "desktop_dir", ""));
    json_set(sub_in, "candidates", json_copy(owned));
    json_set_hdr(sub_in, "platform", json_get_str(in, "platform", "linux"));
    json_t *inst = md2_install_rebuilt_desktop_app(io, sub_in);
    json_free(sub_in);
    if (md2_raised(inst)) { json_free(missing); json_free(owned); return md2_propagate(inst); }
    json_t *installed = json_get(inst, 0);
    json_t *problems = json_get(inst, 1);
    for (size_t i = 0; i < json_len(installed); i++) {
        const char *app = json_get(installed, i)->str_val;
        bool was_missing = false;
        for (size_t k = 0; k < json_len(missing); k++)
            if (strcmp(json_get(missing, k)->str_val, app) == 0) { was_missing = true; break; }
        md2_sbuf b; md2_sb_init(&b);
        if (was_missing) {
            md2_sb_add(&b, "  " MD2_CHECK " Reinstalled the Desktop app at ");
            md2_sb_add(&b, app);
            md2_sb_add(&b, ": it had been removed, so Finder, "
                           "the Dock and Spotlight could not find Hermes");
        } else {
            md2_sb_add(&b, "  " MD2_CHECK " Installed the rebuilt Desktop app at ");
            md2_sb_add(&b, app);
        }
        char *m = md2_sb_take(&b);
        json_t *pr = md2_print(io, m);
        free(m);
        if (md2_raised(pr)) { json_free(inst); json_free(missing); json_free(owned); return md2_propagate(pr); }
        json_free(pr);
    }
    for (size_t i = 0; i < json_len(problems); i++) {
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "  " MD2_WARN " ");
        md2_sb_add(&b, json_get(problems, i)->str_val);
        char *m = md2_sb_take(&b);
        json_t *pr = md2_print(io, m);
        free(m);
        if (md2_raised(pr)) { json_free(inst); json_free(missing); json_free(owned); return md2_propagate(pr); }
        json_free(pr);
    }
    json_free(inst);
    json_t *a2 = json_object();
    json_t *rec_path = md2_call(io, "desktop_install_record", a2);
    json_free(a2);
    if (md2_raised(rec_path)) { json_free(missing); json_free(owned); return md2_propagate(rec_path); }
    json_t *a3 = json_object();
    json_set_hdr(a3, "path", md2_jstr(rec_path));
    json_free(rec_path);
    json_t *rj = md2_call(io, "read_json_or_empty", a3);
    json_free(a3);
    if (md2_raised(rj)) { json_free(missing); json_free(owned); return md2_propagate(rj); }
    if (!md2_is_obj(rj)) {
        const char *tn = md2_is_none(rj) ? "NoneType" : md2_is_arr(rj) ? "list" : "object";
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "'");
        md2_sb_add(&b, tn);
        md2_sb_add(&b, "' object has no attribute 'get'");
        char *m = md2_sb_take(&b);
        json_free(rj);
        json_free(missing);
        json_free(owned);
        json_t *e = md2_raise_desc("AttributeError", m);
        free(m);
        return e;
    }
    json_t *apps = json_array();
    for (size_t i = 0; i < json_len(owned); i++)
        json_append(apps, json_string(json_get(owned, i)->str_val));
    json_free(owned);
    json_t *rec_apps = json_obj_get(rj, "apps");   /* absent -> [] */
    bool same = md2_is_arr(rec_apps) && json_len(rec_apps) == json_len(apps);
    if (same) {
        for (size_t i = 0; i < json_len(apps); i++) {
            json_t *rv = json_get(rec_apps, i);
            json_t *av = json_get(apps, i);
            if (!md2_is_str(rv) || strcmp(rv->str_val, av->str_val) != 0) { same = false; break; }
        }
    } else if (!rec_apps) {
        same = json_len(apps) == 0;   /* .get("apps", []) == [] */
    }
    json_free(rj);
    if (same) { json_free(missing); json_free(apps); return json_null(); }
    /* try: atomic_json_write(desktop_install_record(), {"apps": apps}) */
    json_t *a4 = json_object();
    json_t *rec_path2 = md2_call(io, "desktop_install_record", a4);
    json_free(a4);
    if (md2_raised(rec_path2)) { json_free(missing); json_free(apps); return md2_propagate(rec_path2); }
    json_t *data = json_object();
    json_set(data, "apps", json_copy(apps));
    json_t *a5 = json_object();
    json_set_hdr(a5, "path", md2_jstr(rec_path2));
    json_free(rec_path2);
    json_set(a5, "data", data);
    json_t *aw = md2_call(io, "atomic_json_write", a5);
    json_free(a5);
    json_free(apps);
    json_free(missing);
    if (md2_raised(aw)) {
        if (md2_exc_is_a(md2_exc_type(aw), "OSError")) {
            char *es = md2_exc_str(aw);
            json_free(aw);
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "  " MD2_WARN " Could not record the installed Desktop app (");
            md2_sb_add(&b, es);
            md2_sb_add(&b, ")");
            free(es);
            char *m = md2_sb_take(&b);
            json_t *pr = md2_print(io, m);
            free(m);
            MD2_PROP(pr);
            json_free(pr);
            return json_null();
        }
        return md2_propagate(aw);
    }
    json_free(aw);
    return json_null();
}

/* ── _installed_desktop_launch_target ──────────────────────────────────── */
/* PoP: md2_installed_desktop_launch_target @ hermes_cli/main_desktop.py:_installed_desktop_launch_target */
static json_t *md2_installed_desktop_launch_target(md2_io_t *io, json_t *in) {
    const char *pe = json_get_str(in, "packaged_executable", "");
    if (strcmp(json_get_str(in, "platform", "linux"), "darwin") != 0)
        return json_string(pe);
    json_t *rf = md2_refresh_installed_desktop_apps(io, in);
    MD2_PROP(rf);
    json_free(rf);
    json_t *parents = md2_pparents(pe);
    if (json_len(parents) < 3) {
        json_free(parents);
        return md2_raise_desc("IndexError", "2");
    }
    json_t *hin = json_object();
    json_set_hdr(hin, "app_path", json_get(parents, 2)->str_val);
    json_free(parents);
    json_t *rh = md2_app_asar_hash(io, hin);
    json_free(hin);
    MD2_PROP(rh);
    json_t *apps = md2_installed_desktop_apps(io, in);
    if (md2_raised(apps)) { json_free(rh); return md2_propagate(apps); }
    json_t *ret = json_string(pe);
    if (!md2_is_none(rh)) {
        for (size_t i = 0; i < json_len(apps); i++) {
            const char *app = json_get(apps, i)->str_val;
            json_t *hin2 = json_object();
            json_set_hdr(hin2, "app_path", app);
            json_t *h = md2_app_asar_hash(io, hin2);
            json_free(hin2);
            if (md2_raised(h)) { json_free(apps); json_free(rh); json_free(ret); return md2_propagate(h); }
            bool match = md2_is_str(h) && strcmp(h->str_val, rh->str_val) == 0;
            json_free(h);
            if (match) {
                char *c1 = md2_pjoin(app, "Contents");
                char *c2 = md2_pjoin(c1, "MacOS");
                char *tgt = md2_pjoin(c2, "Hermes");
                free(c1);
                free(c2);
                json_free(ret);
                ret = json_string(tgt);
                free(tgt);
                break;
            }
        }
    }
    json_free(apps);
    json_free(rh);
    return ret;
}
/* ── _promote_staged_desktop_app ───────────────────────────────────────── */
/* PoP: md2_promote_staged_desktop_app @ hermes_cli/main_desktop.py:_promote_staged_desktop_app */
static json_t *md2_promote_staged_desktop_app(md2_io_t *io, json_t *in) {
    const char *desktop_dir = json_get_str(in, "desktop_dir", "");
    const char *staging_dir = json_get_str(in, "staging_dir", "");
    const char *platform = json_get_str(in, "platform", "linux");
    json_t *a = json_object();
    json_set_hdr(a, "release_dir", staging_dir);
    json_t *se = md2_call(io, "_desktop_packaged_executable_in", a);
    json_free(a);
    MD2_PROP(se);
    json_t *a2 = json_object();
    json_set_hdr(a2, "desktop_dir", desktop_dir);
    json_set_hdr(a2, "release_dir", staging_dir);
    json_t *fx = md2_call(io, "_desktop_macos_relaunchable_fixup", a2);
    json_free(a2);
    if (md2_raised(fx)) { json_free(se); return md2_propagate(fx); }
    bool fixup_ok = md2_truth(fx);
    json_free(fx);
    if (!fixup_ok) {
        json_t *din = json_object();
        json_set_hdr(din, "staging_dir", staging_dir);
        json_t *d = md2_discard_desktop_staging(io, din);
        json_free(din);
        if (md2_raised(d)) { json_free(se); return md2_propagate(d); }
        json_free(d);
        json_t *pr = md2_print(io,
            MD2_CROSS " The rebuilt desktop app could not be signed with a stable identity; "
            "not promoting it.");
        if (md2_raised(pr)) { json_free(se); return md2_propagate(pr); }
        json_free(pr);
        json_free(se);
        return md2_raise_desc("RuntimeError",
            "Desktop signing refused the staged build. " MD2_KEPT);
    }
    /* integrity_check: "custom" marker -> custom seam; null + win32 -> the
     * claimed _desktop_exe_integrity_error sibling; else no check. */
    bool custom = md2_is_str(json_obj_get(in, "integrity_check"))
               && strcmp(json_get_str(in, "integrity_check", ""), "custom") == 0;
    bool default_check = !custom && strcmp(platform, "win32") == 0;
    json_t *error = json_null();
    if (!md2_is_none(se) && (custom || default_check)) {
        json_t *ia = json_object();
        json_set_hdr(ia, "path", md2_jstr(se));
        json_free(error);
        error = md2_call(io, custom ? "integrity_check" : "_desktop_exe_integrity_error", ia);
        json_free(ia);
        if (md2_raised(error)) { json_free(se); return md2_propagate(error); }
    }
    if (md2_is_none(se) || !md2_is_none(error)) {
        json_t *din = json_object();
        json_set_hdr(din, "staging_dir", staging_dir);
        json_t *d = md2_discard_desktop_staging(io, din);
        json_free(din);
        if (md2_raised(d)) { json_free(se); json_free(error); return md2_propagate(d); }
        json_free(d);
        md2_sbuf b; md2_sb_init(&b);
        if (md2_is_none(se)) {
            md2_sb_add(&b, MD2_CROSS " Desktop build produced no launchable app in ");
            md2_sb_add(&b, staging_dir);
        } else {
            char *nm = md2_pname(md2_jstr(se));
            md2_sb_add(&b, MD2_CROSS " The built ");
            md2_sb_add(&b, nm);
            free(nm);
            md2_sb_add(&b, " failed its integrity check: ");
            md2_sb_add(&b, md2_jstr(error));
            md2_sb_add(&b, "\n    at: ");
            md2_sb_add(&b, md2_jstr(se));
        }
        char *m = md2_sb_take(&b);
        json_t *pr = md2_print(io, m);
        free(m);
        if (md2_raised(pr)) { json_free(se); json_free(error); return md2_propagate(pr); }
        json_free(pr);
        json_free(se);
        json_free(error);
        return md2_raise_desc("RuntimeError",
            "Desktop build produced no launchable app. " MD2_KEPT);
    }
    json_free(error);
    json_t *held_by = json_array();
    json_t *sa = json_object();
    json_set_hdr(sa, "desktop_dir", desktop_dir);
    json_set_hdr(sa, "staging_dir", staging_dir);
    json_set(sa, "held_by", json_copy(held_by));
    json_t *sw = md2_call(io, "_swap_staged_desktop_app", sa);
    json_free(sa);
    if (md2_raised(sw)) { json_free(se); json_free(held_by); return md2_propagate(sw); }
    json_t *exe = json_obj_get(sw, "exe");
    json_t *hb_ext = json_obj_get(sw, "held_by");
    if (hb_ext && hb_ext->type == JSON_ARRAY) {
        for (size_t i = 0; i < hb_ext->c.count; i++)
            json_append(held_by, json_copy(hb_ext->c.items[i]));
    }
    char *rel = md2_pjoin(desktop_dir, "release");
    if (md2_is_none(exe) && json_len(held_by) > 0) {
        json_t *da = json_object();
        json_set(da, "held_by", json_copy(held_by));
        json_set_hdr(da, "release_dir", rel);
        json_t *db = md2_call(io, "describe_block", da);
        json_free(da);
        if (md2_raised(db)) { free(rel); json_free(se); json_free(held_by); json_free(sw); return md2_propagate(db); }
        json_t *pr = md2_print(io, md2_jstr(db));
        json_free(db);
        if (md2_raised(pr)) { free(rel); json_free(se); json_free(held_by); json_free(sw); return md2_propagate(pr); }
        json_free(pr);
        json_t *ha = json_object();
        json_set(ha, "held_by", json_copy(held_by));
        json_t *hn = md2_call(io, "holder_names", ha);
        json_free(ha);
        if (md2_raised(hn)) { free(rel); json_free(se); json_free(held_by); json_free(sw); return md2_propagate(hn); }
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "Could not publish the desktop build: ");
        md2_sb_add(&b, md2_jstr(hn));
        md2_sb_add(&b, " is using its folder. " MD2_KEPT);
        json_free(hn);
        char *m = md2_sb_take(&b);
        free(rel);
        json_free(se);
        json_free(held_by);
        json_free(sw);
        json_t *out = md2_raise_desc("RuntimeError", m);
        free(m);
        return out;
    }
    if (md2_is_none(exe)) {
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, MD2_CROSS " Could not install the rebuilt desktop app into ");
        md2_sb_add(&b, rel);
        char *m = md2_sb_take(&b);
        json_t *pr = md2_print(io, m);
        free(m);
        if (md2_raised(pr)) { free(rel); json_free(se); json_free(held_by); json_free(sw); return md2_propagate(pr); }
        json_free(pr);
        free(rel);
        json_free(se);
        json_free(held_by);
        json_free(sw);
        return md2_raise_desc("RuntimeError",
            "Could not publish the desktop build. " MD2_KEPT);
    }
    json_t *out = json_string(md2_jstr(exe));
    free(rel);
    json_free(se);
    json_free(held_by);
    json_free(sw);
    return out;
}

/* ── build_prepared_desktop ────────────────────────────────────────────── */
/* PoP: md2_build_prepared_desktop @ hermes_cli/main_desktop.py:build_prepared_desktop */
static json_t *md2_build_prepared_desktop(md2_io_t *io, json_t *in) {
    const char *desktop_dir = json_get_str(in, "desktop_dir", "");
    bool source_mode = md2_truth(json_obj_get(in, "source_mode"));
    const char *pnpm = json_get_str(in, "pnpm", "");
    const char *platform = json_get_str(in, "platform", "linux");
    json_t *pending = NULL;
    json_t *packaged = NULL;
    char *staging_dir = NULL;

    if (!source_mode && strcmp(platform, "win32") == 0) {
        json_t *ain = json_object();
        json_set_hdr(ain, "desktop_dir", desktop_dir);
        json_t *anc = md2_desktop_ancestor_in(io, ain);
        json_free(ain);
        MD2_PROP(anc);
        if (md2_truth(anc)) {
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "  " MD2_WARN " Skipped rebuilding the desktop app: this update is running inside it (pid ");
            md2_sb_addnum(&b, anc->num_val);
            md2_sb_add(&b, "),");
            char *m = md2_sb_take(&b);
            json_t *pr = md2_print(io, m);
            free(m);
            if (md2_raised(pr)) { json_free(anc); return md2_propagate(pr); }
            json_free(pr);
            pr = md2_print(io,
                "    and Windows locks a running app's files. Quit Hermes Desktop and run `hermes desktop`");
            if (md2_raised(pr)) { json_free(anc); return md2_propagate(pr); }
            json_free(pr);
            pr = md2_print(io,
                "    from a terminal, or use Update now in Settings " MD2_ARROW " About, to rebuild and reopen it.");
            json_free(anc);
            if (md2_raised(pr)) return md2_propagate(pr);
            json_free(pr);
            return json_null();
        }
        json_free(anc);
    }
    const char *build_label = source_mode ? "source build" : "packaged app";
    json_t *env_init = json_obj_get(in, "env");
    json_t *build_env = env_init ? json_copy(env_init) : json_object();
    if (strcmp(platform, "win32") == 0) {
        json_t *pa = json_object();
        json_set_hdr(pa, "name", "git");
        json_set(pa, "base_env", json_copy(build_env));
        json_t *pmr = md2_call(io, "pm.ensure", pa);
        json_free(pa);
        if (md2_raised(pmr)) { json_free(build_env); return md2_propagate(pmr); }
        if (!md2_is_obj(pmr)) {
            const char *tn = md2_is_none(pmr) ? "NoneType" : "object";
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "'");
            md2_sb_add(&b, tn);
            md2_sb_add(&b, "' object has no attribute 'env'");
            char *m = md2_sb_take(&b);
            json_free(pmr);
            json_free(build_env);
            json_t *e = md2_raise_desc("AttributeError", m);
            free(m);
            return e;
        }
        json_t *new_env = json_obj_get(pmr, "env");
        json_free(build_env);
        build_env = new_env ? json_copy(new_env) : json_object();
        json_free(pmr);
    }
    json_t *fa = json_object();
    json_set(fa, "env", json_copy(build_env));
    json_set(fa, "source_mode", json_bool(source_mode));
    json_t *forced = md2_call(io, "_force_adhoc_macos_signing", fa);
    json_free(fa);
    if (md2_raised(forced)) { json_free(build_env); return md2_propagate(forced); }
    if (md2_truth(forced)) {
        /* the live sibling's single mutation when it returns True */
        json_set_hdr(build_env, "CSC_IDENTITY_AUTO_DISCOVERY", "false");
        json_t *pr = md2_print(io,
            "  " MD2_ARROW " No Developer ID configured; ad-hoc signing this local rebuild "
            "(CSC_IDENTITY_AUTO_DISCOVERY=false)");
        if (md2_raised(pr)) { json_free(forced); json_free(build_env); return md2_propagate(pr); }
        json_free(pr);
    }
    json_free(forced);
    json_t *build_cmd = json_array();
    json_append(build_cmd, json_string(pnpm));
    json_append(build_cmd, json_string("run"));
    json_append(build_cmd, json_string("build"));
    json_t *icons = json_obj_get(in, "icons");
    if (md2_truth(icons)) {
        json_append(build_cmd, json_string("--icons"));
        char *is = md2_pystr(icons);
        json_append(build_cmd, json_string(is));
        free(is);
    }
    if (!source_mode) {
        json_t *sda = json_object();
        json_set_hdr(sda, "desktop_dir", desktop_dir);
        json_t *sd = md2_call(io, "_desktop_staging_dir", sda);
        json_free(sda);
        if (md2_raised(sd)) { json_free(build_cmd); json_free(build_env); return md2_propagate(sd); }
        staging_dir = md2_dup(md2_jstr(sd));
        json_free(sd);
        json_t *sta = json_object();
        json_set_hdr(sta, "desktop_dir", desktop_dir);
        json_t *stopped = md2_call(io, "_stop_desktop_processes_locking_build", sta);
        json_free(sta);
        if (md2_raised(stopped)) { free(staging_dir); json_free(build_cmd); json_free(build_env); return md2_propagate(stopped); }
        if (json_len(stopped) > 0) {
            md2_sbuf b; md2_sb_init(&b);
            md2_sb_add(&b, "  " MD2_WARN " Stopped running desktop app to free the build output (pid ");
            for (size_t i = 0; i < json_len(stopped); i++) {
                if (i) md2_sb_add(&b, ", ");
                md2_sb_addnum(&b, json_get(stopped, i)->num_val);
            }
            md2_sb_add(&b, ")");
            char *m = md2_sb_take(&b);
            json_t *pr = md2_print(io, m);
            free(m);
            if (md2_raised(pr)) { json_free(stopped); free(staging_dir); json_free(build_cmd); json_free(build_env); return md2_propagate(pr); }
            json_free(pr);
        }
        json_free(stopped);
    }
    json_t *parents = md2_pparents(desktop_dir);
    if (json_len(parents) < 2) {
        json_free(parents);
        free(staging_dir);
        json_free(build_cmd);
        json_free(build_env);
        return md2_raise_desc("IndexError", "1");
    }
    const char *root = json_get(parents, 1)->str_val;
    /* try region */
    {
        md2_sbuf lb; md2_sb_init(&lb);
        md2_sb_add(&lb, "Building desktop ");
        md2_sb_add(&lb, build_label);
        char *label = md2_sb_take(&lb);
        json_t *ca = json_object();
        json_set_hdr(ca, "root", root);
        json_set(ca, "cmd", json_copy(build_cmd));
        json_set_hdr(ca, "label", label);
        free(label);
        json_set_hdr(ca, "cwd", desktop_dir);
        json_set(ca, "env", json_copy(build_env));
        json_t *rc1 = md2_call(io, "run_in_custody", ca);
        json_free(ca);
        if (md2_raised(rc1)) {
            if (md2_exc_is_a(md2_exc_type(rc1), "CalledProcessError")) {
                json_t *da = json_object();
                json_t *outp = json_obj_get(rc1, "output");
                json_set(da, "output", outp ? json_copy(outp) : json_null());
                json_t *dg = md2_call(io, "_diagnose_esbuild_ignore_scripts", da);
                json_free(da);
                if (md2_raised(dg)) { json_free(rc1); pending = dg; goto md2_bpd_finally; }
                json_free(dg);
            }
            pending = rc1;
            goto md2_bpd_finally;
        }
        json_free(rc1);
    }
    if (staging_dir) {
        json_t *psa = json_object();
        json_set_hdr(psa, "label", "Packaging the desktop app");
        json_t *ps = md2_call(io, "publish_stage", psa);
        json_free(psa);
        if (md2_raised(ps)) { pending = ps; goto md2_bpd_finally; }
        json_free(ps);
        json_t *cmd2 = json_array();
        json_append(cmd2, json_string(pnpm));
        json_append(cmd2, json_string("run"));
        json_append(cmd2, json_string("builder"));
        json_append(cmd2, json_string("--dir"));
        json_append(cmd2, json_string("--publish"));
        json_append(cmd2, json_string("never"));
        md2_sbuf ob; md2_sb_init(&ob);
        md2_sb_add(&ob, "-c.directories.output=");
        md2_sb_add(&ob, staging_dir);
        char *oarg = md2_sb_take(&ob);
        json_append(cmd2, json_string(oarg));
        free(oarg);
        json_t *ca = json_object();
        json_set_hdr(ca, "root", root);
        json_set(ca, "cmd", cmd2);
        json_set_hdr(ca, "label", "Packaging the desktop app");
        json_set_hdr(ca, "cwd", desktop_dir);
        json_set(ca, "env", json_copy(build_env));
        json_t *rc2 = md2_call(io, "run_in_custody", ca);
        json_free(ca);
        if (md2_raised(rc2)) {
            if (md2_exc_is_a(md2_exc_type(rc2), "CalledProcessError")) {
                json_t *da = json_object();
                json_t *outp = json_obj_get(rc2, "output");
                json_set(da, "output", outp ? json_copy(outp) : json_null());
                json_t *dg = md2_call(io, "_diagnose_esbuild_ignore_scripts", da);
                json_free(da);
                if (md2_raised(dg)) { json_free(rc2); pending = dg; goto md2_bpd_finally; }
                json_free(dg);
            }
            pending = rc2;
            goto md2_bpd_finally;
        }
        json_free(rc2);
        json_t *pin = json_object();
        json_set_hdr(pin, "desktop_dir", desktop_dir);
        json_set_hdr(pin, "staging_dir", staging_dir);
        json_set_hdr(pin, "platform", platform);
        packaged = md2_promote_staged_desktop_app(io, pin);
        json_free(pin);
        if (md2_raised(packaged)) { pending = packaged; packaged = NULL; goto md2_bpd_finally; }
    }
md2_bpd_finally:
    json_free(parents);
    json_free(build_cmd);
    json_free(build_env);
    if (staging_dir) {
        json_t *din = json_object();
        json_set_hdr(din, "staging_dir", staging_dir);
        json_t *d = md2_discard_desktop_staging(io, din);
        json_free(din);
        free(staging_dir);
        if (md2_raised(d)) {
            if (pending) json_free(pending);
            if (packaged) json_free(packaged);
            return md2_propagate(d);
        }
        json_free(d);
    }
    if (pending) return md2_propagate(pending);
    return packaged ? packaged : json_null();
}

/* ── _desktop_macos_local_codesign ─────────────────────────────────────── */

/* sign_path closure: argv build + subprocess.run(check=True). */
static json_t *md2_sign_path(md2_io_t *io, const char *codesign, const char *identity,
                             const char *path, const char *entitlements,
                             const char *identifier, bool runtime) {
    json_t *args = json_array();
    json_append(args, json_string(codesign));
    json_append(args, json_string("--force"));
    json_append(args, json_string("--sign"));
    json_append(args, json_string(identity));
    json_append(args, json_string("--timestamp=none"));
    if (runtime) {
        json_append(args, json_string("--options"));
        json_append(args, json_string("runtime"));
    }
    if (entitlements) {
        json_t *ex = md2_path_pred(io, "path.exists", entitlements);
        if (md2_raised(ex)) { json_free(args); return ex; }
        bool has = md2_truth(ex);
        json_free(ex);
        if (has) {
            json_append(args, json_string("--entitlements"));
            json_append(args, json_string(entitlements));
        }
    }
    if (identifier && identifier[0] && strcmp(identity, "-") == 0) {
        json_append(args, json_string("--requirements"));
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "=designated => identifier \"");
        md2_sb_add(&b, identifier);
        md2_sb_add(&b, "\"");
        char *req = md2_sb_take(&b);
        json_append(args, json_string(req));
        free(req);
    }
    json_append(args, json_string(path));
    json_t *kw = json_object();
    json_set(kw, "check", json_bool(true));
    json_t *r = md2_subprocess_run(io, args, kw);
    json_free(args);
    json_free(kw);
    return r;
}

/* stable sort of a path-string array by len(parts) DESCENDING. */
static void md2_sort_parts_desc(json_t *arr) {
    size_t n = json_len(arr);
    if (n < 2) return;
    size_t *keys = malloc(n * sizeof(size_t));
    const char **paths = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) {
        paths[i] = json_get(arr, i)->str_val;
        json_t *p = md2_pparts(paths[i]);
        keys[i] = json_len(p);
        json_free(p);
    }
    /* insertion sort: stable, descending key */
    for (size_t i = 1; i < n; i++) {
        size_t k = keys[i];
        const char *v = paths[i];
        size_t j = i;
        while (j > 0 && keys[j - 1] < k) {
            keys[j] = keys[j - 1];
            paths[j] = paths[j - 1];
            j--;
        }
        keys[j] = k;
        paths[j] = v;
    }
    for (size_t i = 0; i < n; i++)
        json_array_set(arr, i, json_string(paths[i]));
    free(keys);
    free(paths);
}

/* PoP: md2_desktop_macos_local_codesign @ hermes_cli/main_desktop.py:_desktop_macos_local_codesign */
static json_t *md2_desktop_macos_local_codesign(md2_io_t *io, json_t *in) {
    const char *app = json_get_str(in, "app", "");
    const char *desktop_dir = json_get_str(in, "desktop_dir", "");
    const char *identity = json_get_str(in, "identity", "-");
    json_t *w = md2_which(io, "codesign");
    MD2_PROP(w);
    if (!md2_truth(w)) { json_free(w); return json_bool(false); }
    const char *codesign = md2_jstr(w);
    char *e1 = md2_pjoin(desktop_dir, "electron");
    char *ent_main = md2_pjoin(e1, "entitlements.mac.plist");
    char *ent_inherit = md2_pjoin(e1, "entitlements.mac.inherit.plist");
    json_t *ex1 = md2_path_pred(io, "path.exists", ent_main);
    if (md2_raised(ex1)) { free(e1); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(ex1); }
    bool has_main = md2_truth(ex1);
    json_free(ex1);
    bool has_inherit = false;
    if (has_main) {
        json_t *ex2 = md2_path_pred(io, "path.exists", ent_inherit);
        if (md2_raised(ex2)) { free(e1); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(ex2); }
        has_inherit = md2_truth(ex2);
        json_free(ex2);
    }
    if (!(has_main && has_inherit)) {
        md2_sbuf b; md2_sb_init(&b);
        md2_sb_add(&b, "desktop entitlement plists missing under ");
        md2_sb_add(&b, e1);
        char *m = md2_sb_take(&b);
        free(e1);
        free(ent_main);
        free(ent_inherit);
        json_free(w);
        json_t *out = md2_raise_desc("FileNotFoundError", m);
        free(m);
        return out;
    }
    free(e1);
    char *contents = md2_pjoin(app, "Contents");
    /* 1) standalone Mach-O files */
    json_t *standalone = json_array();
    json_t *walk1 = md2_os_walk(io, contents);
    if (md2_raised(walk1)) { free(contents); free(ent_main); free(ent_inherit); json_free(w); json_free(standalone); return md2_propagate(walk1); }
    for (size_t i = 0; i < json_len(walk1); i++) {
        json_t *ent = json_get(walk1, i);
        const char *root = json_get_str(ent, "root", "");
        json_t *rel = md2_relative_parts(root, app);
        if (md2_raised(rel)) { json_free(walk1); free(contents); free(ent_main); free(ent_inherit); json_free(w); json_free(standalone); return md2_propagate(rel); }
        bool nested_app = false;
        for (size_t k = 0; k < json_len(rel); k++)
            if (md2_endswith(json_get(rel, k)->str_val, ".app")) { nested_app = true; break; }
        json_free(rel);
        if (nested_app) continue;
        json_t *files = json_obj_get(ent, "files");
        for (size_t k = 0; k < json_len(files); k++) {
            const char *name = json_get(files, k)->str_val;
            char *fp = md2_pjoin(root, name);
            char *suf = md2_psuffix(fp);
            bool pick = strcmp(name, "chrome_crashpad_handler") == 0
                     || strcmp(name, "spawn-helper") == 0
                     || strcmp(suf, ".node") == 0 || strcmp(suf, ".dylib") == 0;
            free(suf);
            if (pick) json_append(standalone, json_string(fp));
            free(fp);
        }
    }
    json_free(walk1);
    md2_sort_parts_desc(standalone);
    for (size_t i = 0; i < json_len(standalone); i++) {
        json_t *r = md2_sign_path(io, codesign, identity, json_get(standalone, i)->str_val,
                                  NULL, NULL, false);
        if (md2_raised(r)) { json_free(standalone); free(contents); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(r); }
        json_free(r);
    }
    json_free(standalone);
    /* 2) nested frameworks and helper apps, deepest first */
    json_t *bundles = json_array();
    char *fw = md2_pjoin(contents, "Frameworks");
    json_t *exfw = md2_path_pred(io, "path.exists", fw);
    if (md2_raised(exfw)) { free(fw); free(contents); free(ent_main); free(ent_inherit); json_free(w); json_free(bundles); return md2_propagate(exfw); }
    bool fw_exists = md2_truth(exfw);
    json_free(exfw);
    if (fw_exists) {
        json_t *walk2 = md2_os_walk(io, fw);
        if (md2_raised(walk2)) { free(fw); free(contents); free(ent_main); free(ent_inherit); json_free(w); json_free(bundles); return md2_propagate(walk2); }
        for (size_t i = 0; i < json_len(walk2); i++) {
            const char *root = json_get_str(json_get(walk2, i), "root", "");
            char *suf = md2_psuffix(root);
            bool pick = strcmp(suf, ".framework") == 0 || strcmp(suf, ".app") == 0;
            free(suf);
            if (!pick) continue;
            bool dup = false;
            for (size_t k = 0; k < json_len(bundles); k++)
                if (strcmp(json_get(bundles, k)->str_val, root) == 0) { dup = true; break; }
            if (!dup) json_append(bundles, json_string(root));
        }
        json_free(walk2);
    }
    free(fw);
    md2_sort_parts_desc(bundles);
    for (size_t i = 0; i < json_len(bundles); i++) {
        const char *bundle = json_get(bundles, i)->str_val;
        json_t *bin = json_object();
        json_set_hdr(bin, "bundle", bundle);
        json_t *bid = md2_desktop_macos_bundle_id(io, bin);
        json_free(bin);
        if (md2_raised(bid)) { json_free(bundles); free(contents); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(bid); }
        json_t *r = md2_sign_path(io, codesign, identity, bundle, ent_inherit,
                                  md2_jstr(bid), true);
        json_free(bid);
        if (md2_raised(r)) { json_free(bundles); free(contents); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(r); }
        json_free(r);
    }
    json_free(bundles);
    /* 3) the main bundle */
    json_t *bin = json_object();
    json_set_hdr(bin, "bundle", app);
    json_t *bid = md2_desktop_macos_bundle_id(io, bin);
    json_free(bin);
    if (md2_raised(bid)) { free(contents); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(bid); }
    json_t *r = md2_sign_path(io, codesign, identity, app, ent_main, md2_jstr(bid), true);
    json_free(bid);
    if (md2_raised(r)) { free(contents); free(ent_main); free(ent_inherit); json_free(w); return md2_propagate(r); }
    json_free(r);
    free(contents);
    free(ent_main);
    free(ent_inherit);
    json_t *vin = json_object();
    json_set_hdr(vin, "codesign", codesign);
    json_set_hdr(vin, "app", app);
    json_t *vkw = json_object();
    json_set(vkw, "check", json_bool(true));
    json_set(vin, "kwargs", vkw);
    json_free(w);
    json_t *cv = md2_codesign_verify(io, vin);
    json_free(vin);
    MD2_PROP(cv);
    json_free(cv);
    return json_bool(true);
}