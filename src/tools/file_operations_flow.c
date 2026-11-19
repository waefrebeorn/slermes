/*
 * file_operations_flow.c — FLOW cores + leaves of tools/file_operations.py
 * (LIVE authority) that NO existing PoP claims: the ShellFileOperations
 * constructor, the cat/head exec leaves, the sentinel-fenced compound read
 * (_fenced_read) and its byte-exact readers (_read_exact_bytes,
 * _read_exact_bytes_hex) with the _bounded/_failed_read/_matches_size static
 * leaves, the stat probe (_probe_regular_file) + _env_unavailable_error,
 * _detect_binary, _native_path, the backend-Python leaves
 * (_python_interpreter_cmd, _exec_python_snippet), read_file_bytes, the
 * pre-write refusal gates (_reject_unencodable, _json_nonstandard_constant,
 * _refuse_introduced_json_constant, _fail_closed_syntax_error),
 * _verify_written_hash, the write_file orchestrator, _no_match_result and
 * _verify_patch_persisted.
 *
 * Self-contained C11, libjson-only. Claimed siblings (_exec,
 * _escape_shell_arg, _new_sentinel, _split_segments,
 * _strip_terminal_fence_leaks, _strip_bom, _has_bom, _normalize_line_endings,
 * _detect_line_ending, _native_read_enabled, _decode_base64_sample,
 * _sample_file_bytes, _is_likely_binary_bytes, _is_likely_binary,
 * _not_regular_error, _expand_path, _probe_write_target, _atomic_write) and
 * every foreign collaborator (has_binary_extension, get_write_denied_error,
 * the LintMixin methods, _native_io.sha256_of, time.monotonic, the
 * fuzzy_match helpers, the LINTERS_INPROC linter callables, and the native
 * read's os.open/os.fstat/open(fd)/fh.read/os.close) enter through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; fo_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is
 * delivered as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised
 * exception). The harness records {"ev": name, "args": args, "ret": result}
 * per call in order; the oracle's recording stubs emit the identical shape
 * driving LIVE Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (fo_fenced_read inside the byte-exact
 * readers, fo_bounded/fo_failed_read/fo_matches_size inside them,
 * fo_probe_regular_file inside read_file_bytes, fo_reject_unencodable /
 * fo_fail_closed_syntax_error / fo_refuse_introduced_json_constant /
 * fo_verify_written_hash inside write_file, fo_read_exact_bytes inside
 * _verify_patch_persisted, fo_python_interpreter_cmd inside
 * _exec_python_snippet, fo_head inside _detect_binary, ...); the oracle keeps
 * the extracted defs real on the same paths, so their internal seam events
 * appear in BOTH traces.
 *
 * Exception propagation: every fo flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. Catch sites transcribe their live kinds:
 * `except OSError` (_read_exact_bytes native block) swallows the OSError
 * family; `except (ValueError, RecursionError)` (_json_nonstandard_constant)
 * and `except (IndexError, ValueError)` (_fenced_read) swallow those families;
 * `except (ValueError, binascii.Error)` (read_file_bytes — binascii.Error IS
 * a ValueError) and `except Exception` (_verify_written_hash,
 * _no_match_result hint) swallow every non-BaseException kind. The
 * BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit) and
 * BaseExceptionGroup ALWAYS pass through. The parent table below carries the
 * FULL builtin hierarchy. Fail-closed: a missing/empty seam script yields a
 * RuntimeError raise spec (recorded, then propagated/swallowed exactly as
 * live); no seam installed at all yields the same spec.
 *
 * Data model:
 *  - bytes travel as lowercase HEX strings ("" == b""); the oracle
 *    materializes them with bytes.fromhex and projects them back to hex.
 *  - ExecuteResult/ReadResult/WriteResult/PatchResult/LintResult travel as
 *    their FULL dataclass field dicts (defaults included); seam scripts may
 *    be partial and are normalized over the defaults (fo_*_from).
 *  - _TooLarge (an ExecuteResult subclass) carries an internal
 *    "__too_large__": true tag for the isinstance(failed, _TooLarge) check;
 *    the harness strips the tag before emission (live projects the plain
 *    ExecuteResult fields).
 *  - tuple returns travel as JSON arrays.
 *  - self state: io->selfj {"cwd": str}; io->env is the terminal_env double
 *    {"cwd": str|null, "config": {"cwd": ...}|null, ...}; io->amb carries
 *    ambient constants (sys_executable, linter_exts, fail_closed_exts).
 *  - os.path is POSIX (posixpath): paths are verbatim POSIX strings; the
 *    oracle seeds an os double backed by posixpath. os.O_RDONLY=0,
 *    os.O_NONBLOCK=0o4000 (POSIX values; the Windows host lacks O_NONBLOCK).
 *
 * Fixture domain (ASCII): paths/commands/content are ASCII (plus the UTF-8
 * BOM U+FEFF and CRLF where noted); integral floats normalize to ints on
 * both sides; max_bytes/file sizes stay non-negative int64; JSON nesting
 * depth <= 100 (the scanner caps at 256, modeling RecursionError);
 * _reject_unencodable content travels as a CODEPOINT ARRAY (list of ints)
 * because lone surrogates cannot round-trip a UTF-8 JSON transport —
 * write_file content stays valid UTF-8 (its surrogate gate never fires in
 * domain; the standalone gate covers the refusal); _verify_patch_persisted
 * re-read bytes stay valid UTF-8 (no surrogateescape output reaches a seam);
 * int()/isdigit() inputs stay ASCII digits/signs/whitespace (no underscores,
 * no Unicode digits); base64 inputs are canonical padded / empty / raising
 * forms (validate=True semantics probed on py3.14).
 *
 * Seam name table (name -> live call):
 *   exec                    self._exec(command, cwd=, timeout=, stdin_data=) [claimed]
 *   escape_shell_arg        self._escape_shell_arg(arg, translate_path=True) [claimed]
 *   new_sentinel            _new_sentinel(prefix) [claimed]
 *   split_segments          _split_segments(output, sentinel) [claimed]
 *   strip_fence             _strip_terminal_fence_leaks(text) [claimed]
 *   strip_bom               _strip_bom(text) [claimed] -> (str, bool)
 *   has_bom                 _has_bom(text) [claimed]
 *   normalize_line_endings  _normalize_line_endings(text, target) [claimed]
 *   native_read_enabled     self._native_read_enabled() [claimed]
 *   decode_base64_sample    self._decode_base64_sample(text) [claimed] -> bytes|null
 *   sample_file_bytes       self._sample_file_bytes(path, length=1000) [claimed] -> bytes|null
 *   is_likely_binary_bytes  self._is_likely_binary_bytes(sample) [claimed]
 *   is_likely_binary        self._is_likely_binary(path, content_sample=) [claimed]
 *   not_regular_error       self._not_regular_error(path) [claimed] -> ReadResult
 *   expand_path             self._expand_path(path) [claimed]
 *   probe_write_target      self._probe_write_target(path, pre_content, want_pre) [claimed]
 *   atomic_write            self._atomic_write(path, content) [claimed] -> ExecuteResult
 *   has_binary_extension    tools.binary_extensions.has_binary_extension(path)
 *   get_write_denied_error  agent.file_safety.get_write_denied_error(path, verb=, entry=)
 *   lsp_local_only          LintMixin._lsp_local_only()
 *   lsp_handles_extension   LintMixin._lsp_handles_extension(ext)
 *   check_lint_delta        LintMixin._check_lint_delta(path, pre_content=, post_content=)
 *   maybe_lsp_diagnostics   LintMixin._maybe_lsp_diagnostics(path, pre_content=, post_content=, started=)
 *   native_sha256           _native_io.sha256_of(path)
 *   monotonic               time.monotonic()
 *   linter                  LINTERS_INPROC[ext](content) -> (ok, err)
 *   is_already_applied      tools.fuzzy_match.is_already_applied(content, old, new)
 *   format_no_match_hint    tools.fuzzy_match.format_no_match_hint(err_msg, n, old, content)
 *   os_open                 os.open(full, O_RDONLY|O_NONBLOCK) -> fd
 *   os_fstat                os.fstat(fd) -> {"st_mode": int}
 *   fh_open                 builtin open(fd, "rb", closefd=False) -> file double
 *   fh_read                 fh.read() / fh.read(n) -> bytes
 *   os_close                os.close(fd)
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*fo_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct fo_io {
    fo_seam_fn call;
    void *ctx;
    const json_t *env;    /* borrowed terminal_env double */
    const json_t *selfj;  /* borrowed self state {"cwd": ...} */
    const json_t *amb;    /* borrowed ambient constants */
} fo_io_t;

static int fo_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *fo_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *fo_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL builtin exception hierarchy (name -> parent) ──
 * Aliases IOError/EnvironmentError/WindowsError ARE OSError; binascii.Error
 * IS a ValueError. ExceptionGroup has TWO parents (BaseExceptionGroup AND
 * Exception) — fo_exc_is walks both chains. Unknown kinds are synthetic
 * Exception subclasses (the oracle's make_exc builds type(kind,(Exception,))). */
typedef struct { const char *name; const char *parent; } fo_exc_ent;
static const fo_exc_ent FO_EXC_TABLE[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"OSError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"UnicodeError", "ValueError"},
    {"JSONDecodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
};

static const char *fo_exc_norm(const char *k) {
    if (strcmp(k, "IOError") == 0 || strcmp(k, "EnvironmentError") == 0 ||
        strcmp(k, "WindowsError") == 0) return "OSError";
    if (strcmp(k, "binascii.Error") == 0) return "ValueError";
    return k;
}

static const char *fo_exc_parent(const char *k) {
    if (strcmp(k, "BaseException") == 0) return NULL;
    for (size_t i = 0; i < sizeof FO_EXC_TABLE / sizeof FO_EXC_TABLE[0]; i++) {
        if (strcmp(FO_EXC_TABLE[i].name, k) == 0) return FO_EXC_TABLE[i].parent;
    }
    return "Exception";   /* synthetic kind */
}

static int fo_exc_is(const char *kind, const char *ancestor) {
    const char *k = fo_exc_norm(kind ? kind : "");
    if (!*k) return 0;
    if (strcmp(k, ancestor) == 0) return 1;
    const char *p = fo_exc_parent(k);
    while (p) {
        if (strcmp(p, ancestor) == 0) return 1;
        p = fo_exc_parent(p);
    }
    /* ExceptionGroup's second parent chain (Exception). */
    if (strcmp(k, "ExceptionGroup") == 0 && strcmp(ancestor, "Exception") == 0) return 1;
    return 0;
}

static int fo_is_exception(const char *k) { return fo_exc_is(k, "Exception"); }
static int fo_is_oserror(const char *k) { return fo_exc_is(k, "OSError"); }
static int fo_is_valueerror(const char *k) { return fo_exc_is(k, "ValueError"); }

static json_t *fo_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *fo_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return fo_raise(kind, buf);
}

static char *fo_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* Universal seam call: args owned by the caller (freed here); result owned by
 * the caller. Fail-closed when no seam is installed. A JSON null result is
 * delivered as a NULL pointer (Python None). */
static json_t *fo_call(fo_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = fo_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int fo_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

/* Python `x is not None` (a present, non-null node). */
static int fo_present(const json_t *v) { return v && v->type != JSON_NULL; }

/* Scalar string reader (json_get_str is an OBJECT+key accessor). */
static const char *fo_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const char *fo_env_str(const fo_io_t *io, const char *key) {
    const json_t *v = (io && io->env) ? json_obj_get(io->env, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const char *fo_self_cwd(const fo_io_t *io) {
    const json_t *v = (io && io->selfj) ? json_obj_get(io->selfj, "cwd") : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : "/";
}

/* env.cwd or self.cwd (the `getattr(self.env,'cwd',None) or self.cwd` chain). */
static const char *fo_live_cwd(const fo_io_t *io) {
    const char *c = fo_env_str(io, "cwd");
    if (c && c[0]) return c;
    return fo_self_cwd(io);
}

static const char *fo_amb_str(const fo_io_t *io, const char *key, const char *def) {
    const json_t *v = (io && io->amb) ? json_obj_get(io->amb, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

static int fo_amb_has_ext(const fo_io_t *io, const char *key, const char *ext,
                          const char *const *def, size_t ndef) {
    const json_t *v = (io && io->amb) ? json_obj_get(io->amb, key) : NULL;
    if (v && v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(v); i++) {
            if (strcmp(fo_jstr(json_get(v, i)), ext) == 0) return 1;
        }
        return 0;
    }
    for (size_t i = 0; i < ndef; i++) if (strcmp(def[i], ext) == 0) return 1;
    return 0;
}

static const char *const FO_LINTER_EXTS[] = {".py", ".json", ".yaml", ".yml", ".toml"};
static const char *const FO_FAIL_CLOSED_EXTS[] = {".json", ".yaml", ".yml", ".toml"};

/* ── string helpers (ASCII domain) ── */

/* str.strip() White_Space (ASCII subset): 0x09-0x0D, 0x1C-0x1F, 0x20. */
static int fo_is_strip_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) || c == 0x20;
}

/* int() strips a NARROWER set: White_Space minus 0x1C-0x1F. */
static int fo_is_int_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20;
}

static char *fo_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && fo_is_strip_ws((unsigned char)s[a])) a++;
    while (b > a && fo_is_strip_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* str.split() on whitespace runs -> owned array of owned strings. */
static char **fo_split_ws(const char *s, size_t *count) {
    *count = 0;
    if (!s) return NULL;
    size_t n = strlen(s), cap = 8;
    char **out = (char **)malloc(cap * sizeof(char *));
    size_t i = 0;
    while (i < n) {
        while (i < n && fo_is_strip_ws((unsigned char)s[i])) i++;
        if (i >= n) break;
        size_t start = i;
        while (i < n && !fo_is_strip_ws((unsigned char)s[i])) i++;
        if (*count == cap) { cap *= 2; out = (char **)realloc(out, cap * sizeof(char *)); }
        out[*count] = (char *)malloc(i - start + 1);
        memcpy(out[*count], s + start, i - start);
        out[*count][i - start] = '\0';
        (*count)++;
    }
    return out;
}

static void fo_free_tokens(char **tok, size_t n) {
    for (size_t i = 0; i < n; i++) free(tok[i]);
    free(tok);
}

/* str.isdigit() — ASCII digits only in domain (non-empty, all 0-9). */
static int fo_isdigit(const char *s) {
    if (!s || !s[0]) return 0;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return 0;
    return 1;
}

/* int(token) for the ASCII digit/sign domain. Returns 0 on failure. */
static long long fo_py_int(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && fo_is_int_ws((unsigned char)s[a])) a++;
    while (b > a && fo_is_int_ws((unsigned char)s[b - 1])) b--;
    if (a >= b) return 0;
    int neg = 0;
    if (s[a] == '+' || s[a] == '-') { neg = (s[a] == '-'); a++; }
    if (a >= b) return 0;
    long long v = 0;
    for (size_t i = a; i < b; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *ok = 1;
    return neg ? -v : v;
}

/* f"{n:,}" — thousands-separated decimal. */
static char *fo_comma(long long n) {
    char digits[32];
    int neg = n < 0;
    unsigned long long u = neg ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    int len = snprintf(digits, sizeof digits, "%llu", u);
    int commas = (len - 1) / 3;
    char *out = (char *)malloc((size_t)(len + commas + neg + 1));
    int pos = 0;
    if (neg) out[pos++] = '-';
    for (int i = 0; i < len; i++) {
        out[pos++] = digits[i];
        int rem = len - 1 - i;
        if (rem > 0 && rem % 3 == 0) out[pos++] = ',';
    }
    out[pos] = '\0';
    return out;
}

static char *fo_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static char *fo_concat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *out = (char *)malloc(la + lb + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return out;
}

/* ── POSIX path algebra (posixpath semantics) ── */

static int fo_isabs(const char *p) { return p && p[0] == '/'; }

static char *fo_join(const char *a, const char *b) {
    if (fo_isabs(b)) return strdup(b);
    if (!a || !a[0]) return strdup(b);
    size_t la = strlen(a);
    int need_sep = (a[la - 1] != '/');
    char *out = (char *)malloc(la + strlen(b) + 2);
    sprintf(out, "%s%s%s", a, need_sep ? "/" : "", b);
    return out;
}

static char *fo_dirname(const char *p) {
    const char *last = strrchr(p, '/');
    size_t i = last ? (size_t)(last - p) + 1 : 0;
    char *head = (char *)malloc(i + 1);
    memcpy(head, p, i);
    head[i] = '\0';
    if (i > 0) {
        int all_slash = 1;
        for (size_t k = 0; k < i; k++) if (head[k] != '/') { all_slash = 0; break; }
        if (!all_slash) {
            while (i > 0 && head[i - 1] == '/') i--;
            head[i] = '\0';
        }
    }
    return head;
}

/* posixpath.splitext: leading dots in the basename are not an extension. */
static void fo_splitext(const char *p, char **root, char **ext) {
    const char *sep = strrchr(p, '/');
    const char *dot = strrchr(p, '.');
    long sep_i = sep ? (long)(sep - p) : -1;
    long dot_i = dot ? (long)(dot - p) : -1;
    if (dot_i > sep_i) {
        long fi = sep_i + 1;
        while (fi < dot_i) {
            if (p[fi] != '.') {
                *root = strndup(p, (size_t)dot_i);
                *ext = strdup(p + dot_i);
                return;
            }
            fi++;
        }
    }
    *root = strdup(p);
    *ext = strdup("");
}

/* ── bytes helpers (hex transport) ── */

typedef struct { unsigned char *data; size_t len; } fo_bytes;

static int fo_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* bytes.fromhex on a whitespace-free string: even length + hex digits. */
static fo_bytes fo_bytes_fromhex(const char *s, int *ok) {
    fo_bytes b = {NULL, 0};
    *ok = 0;
    size_t n = strlen(s);
    if (n % 2 != 0) return b;
    for (size_t i = 0; i < n; i++) if (fo_hexval(s[i]) < 0) return b;
    b.data = (unsigned char *)malloc(n / 2 + 1);
    b.len = n / 2;
    for (size_t i = 0; i < b.len; i++)
        b.data[i] = (unsigned char)((fo_hexval(s[2 * i]) << 4) | fo_hexval(s[2 * i + 1]));
    *ok = 1;
    return b;
}

static char *fo_hex_encode(const unsigned char *data, size_t len) {
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(len * 2 + 1);
    for (size_t i = 0; i < len; i++) {
        out[2 * i] = hx[data[i] >> 4];
        out[2 * i + 1] = hx[data[i] & 0xF];
    }
    out[len * 2] = '\0';
    return out;
}

/* base64.b64decode(s, validate=True) accept/reject (py3.14 probed):
 * alphabet [A-Za-z0-9+/] plus trailing '='; no leading/discontinuous padding;
 * at most 2 pad chars; total length a multiple of 4; data count % 4 != 1. */
static int fo_b64_valid(const char *s) {
    size_t n = strlen(s);
    if (n == 0) return 1;
    size_t first_pad = n;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '+' || c == '/';
        if (c == '=') { if (first_pad == n) first_pad = i; }
        else if (!alpha) return 0;
        else if (first_pad != n) return 0;   /* data after padding */
    }
    if (first_pad == n) return (n % 4 == 0);
    if (first_pad == 0) return 0;            /* leading padding */
    size_t pad = n - first_pad;
    if (pad > 2) return 0;                   /* excess padding */
    if (first_pad % 4 == 1) return 0;        /* 1 mod 4 data chars */
    return (n % 4 == 0);
}

static char *fo_b64_encode(const unsigned char *data, size_t len) {
    static const char al[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = ((len + 2) / 3) * 4;
    char *out = (char *)malloc(out_len + 1);
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned v = (unsigned)data[i] << 16;
        if (i + 1 < len) v |= (unsigned)data[i + 1] << 8;
        if (i + 2 < len) v |= (unsigned)data[i + 2];
        out[o++] = al[(v >> 18) & 63];
        out[o++] = al[(v >> 12) & 63];
        out[o++] = (i + 1 < len) ? al[(v >> 6) & 63] : '=';
        out[o++] = (i + 2 < len) ? al[v & 63] : '=';
    }
    out[o] = '\0';
    return out;
}

/* ── SHA-256 (self-contained; probed against hashlib) ── */

typedef struct { unsigned int h[8]; unsigned long long len; unsigned char buf[64]; size_t buflen; } fo_sha_ctx;

static unsigned int fo_rotr(unsigned int x, int n) { return (x >> n) | (x << (32 - n)); }

static void fo_sha_block(fo_sha_ctx *c, const unsigned char *p) {
    static const unsigned int K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    unsigned int w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((unsigned)p[4*i] << 24) | ((unsigned)p[4*i+1] << 16) |
               ((unsigned)p[4*i+2] << 8) | (unsigned)p[4*i+3];
    for (int i = 16; i < 64; i++) {
        unsigned s0 = fo_rotr(w[i-15],7) ^ fo_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        unsigned s1 = fo_rotr(w[i-2],17) ^ fo_rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    unsigned a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        unsigned S1 = fo_rotr(e,6) ^ fo_rotr(e,11) ^ fo_rotr(e,25);
        unsigned ch = (e & f) ^ ((~e) & g);
        unsigned t1 = h + S1 + ch + K[i] + w[i];
        unsigned S0 = fo_rotr(a,2) ^ fo_rotr(a,13) ^ fo_rotr(a,22);
        unsigned maj = (a & b) ^ (a & cc) ^ (b & cc);
        unsigned t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void fo_sha_update(fo_sha_ctx *c, const unsigned char *data, size_t len) {
    c->len += len;
    while (len > 0) {
        size_t take = 64 - c->buflen;
        if (take > len) take = len;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        len -= take;
        if (c->buflen == 64) { fo_sha_block(c, c->buf); c->buflen = 0; }
    }
}

static char *fo_sha256_hex(const unsigned char *data, size_t len) {
    fo_sha_ctx c = {{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}, 0, {0}, 0};
    fo_sha_update(&c, data, len);
    unsigned long long bits = c.len * 8ULL;
    unsigned char pad[72];
    size_t plen = 0;
    pad[plen++] = 0x80;
    while ((c.buflen + plen) % 64 != 56) pad[plen++] = 0;
    for (int i = 7; i >= 0; i--) pad[plen++] = (unsigned char)(bits >> (8 * i));
    fo_sha_update(&c, pad, plen);
    char *out = (char *)malloc(65);
    for (int i = 0; i < 8; i++) snprintf(out + 8 * i, 9, "%08x", c.h[i]);
    out[64] = '\0';
    return out;
}

/* ── UTF-8 codepoint helpers (surrogateescape) ── */

/* Decode UTF-8 with errors="surrogateescape": each undecodable byte becomes
 * U+DC00+byte. Returns a malloc'd codepoint array; *n gets the count. */
static unsigned int *fo_utf8_decode(const unsigned char *s, size_t len, size_t *n) {
    unsigned int *out = (unsigned int *)malloc((len + 1) * sizeof(unsigned int));
    size_t o = 0, i = 0;
    while (i < len) {
        unsigned char c = s[i];
        if (c < 0x80) { out[o++] = c; i++; continue; }
        int need = 0; unsigned int cp = 0, lo = 0, hi = 0;
        if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; lo = 0x80; hi = 0xBF; }
        else if (c >= 0xE0 && c <= 0xEF) { need = 2; cp = c & 0x0F; lo = (c == 0xE0) ? 0xA0 : 0x80; hi = (c == 0xED) ? 0x9F : 0xBF; }
        else if (c >= 0xF0 && c <= 0xF4) { need = 3; cp = c & 0x07; lo = (c == 0xF0) ? 0x90 : 0x80; hi = (c == 0xF4) ? 0x8F : 0xBF; }
        else { out[o++] = 0xDC00 + c; i++; continue; }
        if (i + (size_t)need >= len) { out[o++] = 0xDC00 + c; i++; continue; }
        int bad = 0;
        unsigned int v = cp;
        for (int k = 1; k <= need; k++) {
            unsigned char cc = s[i + (size_t)k];
            unsigned char l = (k == 1) ? (unsigned char)lo : 0x80;
            unsigned char h = (k == 1) ? (unsigned char)hi : 0xBF;
            if (cc < l || cc > h) { bad = 1; break; }
            v = (v << 6) | (cc & 0x3F);
        }
        if (bad) { out[o++] = 0xDC00 + c; i++; continue; }
        out[o++] = v;
        i += (size_t)need + 1;
    }
    *n = o;
    return out;
}

/* Encode codepoints with errors="surrogateescape": U+DC80-U+DCFF -> the
 * original byte. Other lone surrogates cannot occur (gated upstream). */
static unsigned char *fo_utf8_encode(const unsigned int *cps, size_t n, size_t *out_len) {
    unsigned char *out = (unsigned char *)malloc(n * 4 + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned int cp = cps[i];
        if (cp >= 0xDC80 && cp <= 0xDCFF) { out[o++] = (unsigned char)(cp - 0xDC00); }
        else if (cp < 0x80) { out[o++] = (unsigned char)cp; }
        else if (cp < 0x800) {
            out[o++] = (unsigned char)(0xC0 | (cp >> 6));
            out[o++] = (unsigned char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[o++] = (unsigned char)(0xE0 | (cp >> 12));
            out[o++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (unsigned char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (unsigned char)(0xF0 | (cp >> 18));
            out[o++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (unsigned char)(0x80 | (cp & 0x3F));
        }
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

/* Codepoint count of a valid UTF-8 string (len() in chars). */
static size_t fo_cp_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* ── result dataclass builders (FULL field sets, declaration order) ── */

static json_t *fo_exec_result(const char *out, long long code, const char *cwd_err) {
    json_t *r = json_object();
    json_set(r, "stdout", json_string(out ? out : ""));
    json_set(r, "exit_code", json_number((double)code));
    json_set(r, "cwd_error", json_string(cwd_err ? cwd_err : ""));
    return r;
}

/* Normalize a (possibly partial) exec seam script over the defaults; keep the
 * internal __too_large__ tag (the isinstance(failed, _TooLarge) model). */
static json_t *fo_exec_from(const json_t *r) {
    json_t *o = fo_exec_result(json_get_str(r, "stdout", ""),
                               (long long)json_get_num(r, "exit_code", 0),
                               json_get_str(r, "cwd_error", ""));
    if (r && json_obj_get(r, "__too_large__")) json_set(o, "__too_large__", json_bool(true));
    return o;
}

static int fo_is_too_large(const json_t *r) {
    const json_t *t = r ? json_obj_get(r, "__too_large__") : NULL;
    return t && t->type == JSON_BOOL && t->bool_val;
}

static json_t *fo_read_result(void) {
    json_t *r = json_object();
    json_set(r, "content", json_string(""));
    json_set(r, "total_lines", json_number(0));
    json_set(r, "file_size", json_number(0));
    json_set(r, "truncated", json_bool(false));
    json_set(r, "truncated_lines", json_null());
    json_set(r, "hint", json_null());
    json_set(r, "is_binary", json_bool(false));
    json_set(r, "is_image", json_bool(false));
    json_set(r, "base64_content", json_null());
    json_set(r, "mime_type", json_null());
    json_set(r, "dimensions", json_null());
    json_set(r, "error", json_null());
    json_set(r, "not_found", json_bool(false));
    json_set(r, "similar_files", json_array());
    json_set(r, "_snapshot", json_null());
    json_set(r, "_content_sha256", json_null());
    json_set(r, "_too_large", json_bool(false));
    return r;
}

static json_t *fo_write_result(void) {
    json_t *r = json_object();
    json_set(r, "bytes_written", json_number(0));
    json_set(r, "dirs_created", json_bool(false));
    json_set(r, "verified", json_null());
    json_set(r, "_content_sha256", json_null());
    json_set(r, "lint", json_null());
    json_set(r, "lsp_diagnostics", json_null());
    json_set(r, "error", json_null());
    json_set(r, "warning", json_null());
    return r;
}

static json_t *fo_patch_result(void) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "diff", json_string(""));
    json_set(r, "files_modified", json_array());
    json_set(r, "files_created", json_array());
    json_set(r, "files_deleted", json_array());
    json_set(r, "lint", json_null());
    json_set(r, "lsp_diagnostics", json_null());
    json_set(r, "error", json_null());
    json_set(r, "no_change", json_bool(false));
    json_set(r, "note", json_null());
    json_set(r, "_writes", json_array());
    return r;
}

static json_t *fo_lint_result(void) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(true));
    json_set(r, "skipped", json_bool(false));
    json_set(r, "output", json_string(""));
    json_set(r, "message", json_string(""));
    return r;
}

/* Overlay a partial dataclass script onto the full default field set. */
static json_t *fo_overlay(json_t *base, const json_t *script) {
    if (script && script->type == JSON_OBJECT) {
        for (size_t i = 0; i < script->c.count; i++)
            json_set(base, script->c.keys[i], json_copy(script->c.items[i]));
    }
    return base;
}

static json_t *fo_read_from(const json_t *r) { return fo_overlay(fo_read_result(), r); }
static json_t *fo_lint_from(const json_t *r) { return fo_overlay(fo_lint_result(), r); }

/* LintResult.to_dict() */
static json_t *fo_lint_to_dict(const json_t *l) {
    json_t *d = json_object();
    if (json_get_bool(l, "skipped", false)) {
        json_set(d, "status", json_string("skipped"));
        json_set(d, "message", json_string(json_get_str(l, "message", "")));
        return d;
    }
    json_set(d, "status", json_string(json_get_bool(l, "success", true) ? "ok" : "error"));
    json_set(d, "output", json_string(json_get_str(l, "output", "")));
    const char *msg = json_get_str(l, "message", "");
    if (msg[0]) json_set(d, "message", json_string(msg));
    return d;
}

static json_t *fo_tuple2(json_t *a, json_t *b) {
    json_t *t = json_array();
    json_append(t, a ? a : json_null());
    json_append(t, b ? b : json_null());
    return t;
}

static json_t *fo_tuple3(json_t *a, json_t *b, json_t *c) {
    json_t *t = json_array();
    json_append(t, a ? a : json_null());
    json_append(t, b ? b : json_null());
    json_append(t, c ? c : json_null());
    return t;
}

/* ── strict JSON scanner (Python json.loads grammar + parse_constant order) ──
 * Validates the text and records NaN/Infinity/-Infinity tokens in encounter
 * (depth-first, left-to-right) order. libjson's json_parse is LENIENT, so the
 * grammar is checked here BEFORE any value is trusted. Depth cap models the
 * RecursionError catch (fixtures stay <= 100 deep). */

typedef struct { const char *s; size_t n, pos; int depth, bad; json_t *consts; } fo_jscan;

static void fo_j_ws(fo_jscan *j) {
    while (j->pos < j->n) {
        char c = j->s[j->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') j->pos++;
        else break;
    }
}

static int fo_j_lit(fo_jscan *j, const char *lit) {
    size_t ln = strlen(lit);
    if (j->pos + ln > j->n || memcmp(j->s + j->pos, lit, ln) != 0) { j->bad = 1; return 0; }
    j->pos += ln;
    return 1;
}

static void fo_j_string(fo_jscan *j) {
    if (j->pos >= j->n || j->s[j->pos] != '"') { j->bad = 1; return; }
    j->pos++;
    while (j->pos < j->n) {
        unsigned char c = (unsigned char)j->s[j->pos];
        if (c == '"') { j->pos++; return; }
        if (c < 0x20) { j->bad = 1; return; }   /* strict: raw control char */
        if (c == '\\') {
            j->pos++;
            if (j->pos >= j->n) { j->bad = 1; return; }
            char e = j->s[j->pos];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' ||
                e == 'n' || e == 'r' || e == 't') { j->pos++; }
            else if (e == 'u') {
                j->pos++;
                if (j->pos + 4 > j->n) { j->bad = 1; return; }
                for (int k = 0; k < 4; k++)
                    if (fo_hexval(j->s[j->pos + (size_t)k]) < 0) { j->bad = 1; return; }
                j->pos += 4;
            } else { j->bad = 1; return; }
        } else j->pos++;
    }
    j->bad = 1;   /* unterminated */
}

static void fo_j_number(fo_jscan *j) {
    if (j->pos < j->n && j->s[j->pos] == '-') j->pos++;
    if (j->pos >= j->n) { j->bad = 1; return; }
    if (j->s[j->pos] == '0') j->pos++;
    else if (j->s[j->pos] >= '1' && j->s[j->pos] <= '9') {
        while (j->pos < j->n && j->s[j->pos] >= '0' && j->s[j->pos] <= '9') j->pos++;
    } else { j->bad = 1; return; }
    if (j->pos < j->n && j->s[j->pos] == '.') {
        j->pos++;
        if (j->pos >= j->n || j->s[j->pos] < '0' || j->s[j->pos] > '9') { j->bad = 1; return; }
        while (j->pos < j->n && j->s[j->pos] >= '0' && j->s[j->pos] <= '9') j->pos++;
    }
    if (j->pos < j->n && (j->s[j->pos] == 'e' || j->s[j->pos] == 'E')) {
        j->pos++;
        if (j->pos < j->n && (j->s[j->pos] == '+' || j->s[j->pos] == '-')) j->pos++;
        if (j->pos >= j->n || j->s[j->pos] < '0' || j->s[j->pos] > '9') { j->bad = 1; return; }
        while (j->pos < j->n && j->s[j->pos] >= '0' && j->s[j->pos] <= '9') j->pos++;
    }
}

static void fo_j_value(fo_jscan *j) {
    if (j->bad) return;
    fo_j_ws(j);
    if (j->pos >= j->n) { j->bad = 1; return; }
    char c = j->s[j->pos];
    if (c == '{') {
        if (++j->depth > 256) { j->bad = 1; return; }
        j->pos++;
        fo_j_ws(j);
        if (j->pos < j->n && j->s[j->pos] == '}') { j->pos++; j->depth--; return; }
        for (;;) {
            fo_j_ws(j);
            fo_j_string(j);
            if (j->bad) return;
            fo_j_ws(j);
            if (j->pos >= j->n || j->s[j->pos] != ':') { j->bad = 1; return; }
            j->pos++;
            fo_j_value(j);
            if (j->bad) return;
            fo_j_ws(j);
            if (j->pos < j->n && j->s[j->pos] == ',') {
                j->pos++;
                fo_j_ws(j);
                if (j->pos < j->n && j->s[j->pos] == '}') { j->bad = 1; return; }
                continue;
            }
            if (j->pos < j->n && j->s[j->pos] == '}') { j->pos++; j->depth--; return; }
            j->bad = 1; return;
        }
    }
    if (c == '[') {
        if (++j->depth > 256) { j->bad = 1; return; }
        j->pos++;
        fo_j_ws(j);
        if (j->pos < j->n && j->s[j->pos] == ']') { j->pos++; j->depth--; return; }
        for (;;) {
            fo_j_value(j);
            if (j->bad) return;
            fo_j_ws(j);
            if (j->pos < j->n && j->s[j->pos] == ',') {
                j->pos++;
                fo_j_ws(j);
                if (j->pos < j->n && j->s[j->pos] == ']') { j->bad = 1; return; }
                continue;
            }
            if (j->pos < j->n && j->s[j->pos] == ']') { j->pos++; j->depth--; return; }
            j->bad = 1; return;
        }
    }
    if (c == '"') { fo_j_string(j); return; }
    if (c == 't') { fo_j_lit(j, "true"); return; }
    if (c == 'f') { fo_j_lit(j, "false"); return; }
    if (c == 'n') { fo_j_lit(j, "null"); return; }
    if (c == 'N') { if (fo_j_lit(j, "NaN")) json_append(j->consts, json_string("NaN")); return; }
    if (c == 'I') { if (fo_j_lit(j, "Infinity")) json_append(j->consts, json_string("Infinity")); return; }
    if (c == '-' && j->pos + 9 <= j->n && memcmp(j->s + j->pos, "-Infinity", 9) == 0) {
        j->pos += 9;
        json_append(j->consts, json_string("-Infinity"));
        return;
    }
    if (c == '-' || (c >= '0' && c <= '9')) { fo_j_number(j); return; }
    j->bad = 1;
}

/* ── ported functions ── */

/* PoP: fo_init @ tools/file_operations.py:__init__ */
static json_t *fo_init(fo_io_t *io, const json_t *terminal_env, const json_t *cwd_arg) {
    (void)io;
    const char *c = NULL;
    if (fo_truthy(cwd_arg)) c = fo_jstr(cwd_arg);
    if (!c || !c[0]) {
        const json_t *v = terminal_env ? json_obj_get(terminal_env, "cwd") : NULL;
        if (fo_truthy(v)) c = fo_jstr(v);
    }
    if (!c || !c[0]) {
        const json_t *cfg = terminal_env ? json_obj_get(terminal_env, "config") : NULL;
        const json_t *v = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "cwd") : NULL;
        if (fo_truthy(v)) c = fo_jstr(v);
    }
    if (!c || !c[0]) c = "/";
    json_t *self = json_object();
    json_set(self, "cwd", json_string(c));
    json_set(self, "_command_cache", json_object());
    json_set(self, "_rg_resolution_cache", json_object());
    json_set(self, "_rg_modified_capability", json_object());
    return self;
}

/* PoP: fo_cat @ tools/file_operations.py:_cat */
static json_t *fo_cat(fo_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set(a, "arg", json_string(path));
    json_set(a, "translate_path", json_bool(true));
    json_t *q = fo_call(io, "escape_shell_arg", a);
    if (fo_is_raise(q)) return q;
    char *cmd = fo_fmt("cat %s 2>/dev/null", fo_jstr(q));
    json_free(q);
    json_t *ea = json_object();
    json_set(ea, "command", json_string(cmd));
    free(cmd);
    json_t *r = fo_call(io, "exec", ea);
    if (fo_is_raise(r)) return r;
    json_t *out = fo_exec_from(r);
    json_free(r);
    return out;
}

/* PoP: fo_head @ tools/file_operations.py:_head */
static json_t *fo_head(fo_io_t *io, const char *path, long long nbytes) {
    json_t *a = json_object();
    json_set(a, "arg", json_string(path));
    json_set(a, "translate_path", json_bool(true));
    json_t *q = fo_call(io, "escape_shell_arg", a);
    if (fo_is_raise(q)) return q;
    char *cmd = fo_fmt("head -c %lld %s 2>/dev/null", nbytes, fo_jstr(q));
    json_free(q);
    json_t *ea = json_object();
    json_set(ea, "command", json_string(cmd));
    free(cmd);
    json_t *r = fo_call(io, "exec", ea);
    if (fo_is_raise(r)) return r;
    json_t *out = fo_exec_from(r);
    json_free(r);
    return out;
}

/* PoP: fo_bounded @ tools/file_operations.py:_bounded */
static char *fo_bounded(const char *encoder, const char *arg, const json_t *max_bytes) {
    if (!fo_present(max_bytes)) return fo_fmt("%s < %s", encoder, arg);
    long long mb = (long long)max_bytes->num_val;
    return fo_fmt("{ head -c %lld | %s; } < %s", mb + 1, encoder, arg);
}

/* PoP: fo_failed_read @ tools/file_operations.py:_failed_read */
static json_t *fo_failed_read(fo_io_t *io, const char *path, const char *payload, long long read_rc) {
    json_t *a = json_object();
    json_set(a, "text", json_string(payload));
    json_t *r = fo_call(io, "strip_fence", a);
    if (fo_is_raise(r)) return r;
    char *stripped = fo_strip(fo_jstr(r));
    json_free(r);
    char *out;
    if (stripped[0]) out = strdup(stripped);
    else out = fo_fmt("%s: exit %lld", path, read_rc);
    free(stripped);
    json_t *res = fo_exec_result(out, read_rc, "");
    free(out);
    return res;
}

/* PoP: fo_matches_size @ tools/file_operations.py:_matches_size */
static json_t *fo_matches_size(fo_io_t *io, const char *data_hex, const char *size_segment) {
    int ok;
    fo_bytes data = fo_bytes_fromhex(data_hex, &ok);
    json_t *a = json_object();
    json_set(a, "text", json_string(size_segment));
    json_t *r = fo_call(io, "strip_fence", a);
    if (fo_is_raise(r)) { free(data.data); return r; }
    size_t nt = 0;
    char **tok = fo_split_ws(fo_jstr(r), &nt);
    json_free(r);
    json_t *ret = NULL;
    if (nt == 1 && fo_isdigit(tok[0])) {
        int iok;
        long long v = fo_py_int(tok[0], &iok);
        if (!iok) ret = fo_raise_fmt("ValueError", "invalid literal for int() with base 10: '%s'", tok[0]);
        else ret = json_bool(v == (long long)data.len);
    } else ret = json_bool(false);
    fo_free_tokens(tok, nt);
    free(data.data);
    return ret;
}

/* PoP: fo_fenced_read @ tools/file_operations.py:_fenced_read */
static json_t *fo_fenced_read(fo_io_t *io, const char *body, const json_t *more) {
    json_t *sa = json_object();
    json_set(sa, "prefix", json_string("__HERMES_RB_"));
    json_t *sent = fo_call(io, "new_sentinel", sa);
    if (fo_is_raise(sent)) return sent;
    char *mark = fo_fmt("echo %s", fo_jstr(sent));
    size_t nm = more ? json_len(more) : 0;
    char *rest = strdup("");
    for (size_t i = 0; i < nm; i++) {
        char *piece = fo_fmt("%s; %s; ", mark, fo_jstr(json_get(more, i)));
        char *nr = fo_concat(rest, piece);
        free(rest); free(piece);
        rest = nr;
    }
    char *cmd = fo_fmt("{ set +x; } 2>/dev/null; %s; %s; __hb=$?; %s%s; echo $__hb",
                       mark, body, rest, mark);
    free(rest); free(mark);
    json_t *ea = json_object();
    json_set(ea, "command", json_string(cmd));
    free(cmd);
    json_t *result = fo_call(io, "exec", ea);
    if (fo_is_raise(result)) { json_free(sent); return result; }
    json_t *exec = fo_exec_from(result);
    json_free(result);
    json_t *spa = json_object();
    json_set(spa, "output", json_string(json_get_str(exec, "stdout", "")));
    json_set(spa, "sentinel", json_string(fo_jstr(sent)));
    json_free(sent);
    json_t *segs = fo_call(io, "split_segments", spa);
    if (fo_is_raise(segs)) { json_free(exec); return segs; }
    size_t ns = segs ? json_len(segs) : 0;
    if (!segs || segs->type != JSON_ARRAY || ns != nm + 3) {
        json_free(segs);
        return fo_tuple3(NULL, NULL, exec);
    }
    json_t *mid = json_array();
    for (size_t i = 1; i + 1 < ns; i++) json_append(mid, json_copy(json_get(segs, i)));
    json_t *fa = json_object();
    json_set(fa, "text", json_copy(json_get(segs, ns - 1)));
    json_free(segs);
    json_t *fr = fo_call(io, "strip_fence", fa);
    json_t *rc = NULL;
    if (fo_is_raise(fr)) {
        const char *k = fo_raise_kind(fr);
        if (fo_exc_is(k, "IndexError") || fo_is_valueerror(k)) { json_free(fr); }
        else { json_free(mid); json_free(exec); return fr; }
    } else if (!fr) {
        json_free(mid); json_free(exec);
        return fo_raise("AttributeError", "'NoneType' object has no attribute 'split'");
    } else {
        size_t nt = 0;
        char **tok = fo_split_ws(fo_jstr(fr), &nt);
        json_free(fr);
        if (nt > 0) {
            int iok;
            long long v = fo_py_int(tok[0], &iok);
            if (iok) rc = json_number((double)v);
        }
        fo_free_tokens(tok, nt);
    }
    return fo_tuple3(mid, rc, exec);
}

/* PoP: fo_probe_regular_file @ tools/file_operations.py:_probe_regular_file */
static json_t *fo_probe_regular_file(fo_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set(a, "arg", json_string(path));
    json_set(a, "translate_path", json_bool(true));
    json_t *q = fo_call(io, "escape_shell_arg", a);
    if (fo_is_raise(q)) return q;
    char *cmd = fo_fmt(
        "if [ -f %s ]; then wc -c < %s 2>/dev/null; "
        "elif [ -e %s ] || [ -L %s ]; then echo __hermes_not_regular__; "
        "else echo __hermes_missing__; fi", fo_jstr(q), fo_jstr(q), fo_jstr(q), fo_jstr(q));
    json_free(q);
    json_t *ea = json_object();
    json_set(ea, "command", json_string(cmd));
    free(cmd);
    json_t *r = fo_call(io, "exec", ea);
    if (fo_is_raise(r)) return r;
    json_t *exec = fo_exec_from(r);
    json_free(r);
    json_t *fa = json_object();
    json_set(fa, "text", json_string(json_get_str(exec, "stdout", "")));
    json_t *fr = fo_call(io, "strip_fence", fa);
    if (fo_is_raise(fr)) { json_free(exec); return fr; }
    char *stat_output = fo_strip(fo_jstr(fr));
    json_free(fr);
    json_t *ret = NULL;
    if (strcmp(stat_output, "__hermes_missing__") == 0)
        ret = fo_tuple2(json_number(0), json_string("missing"));
    else if (strcmp(stat_output, "__hermes_not_regular__") == 0)
        ret = fo_tuple2(json_number(0), json_string("not_regular"));
    else if ((long long)json_get_num(exec, "exit_code", 0) != 0) {
        const char *ce = json_get_str(exec, "cwd_error", "");
        ret = fo_tuple2(json_number(0), json_string(ce[0] ? ce : "env_unavailable"));
    } else {
        int iok;
        long long v = fo_py_int(stat_output, &iok);
        if (iok) ret = fo_tuple2(json_number((double)v), json_string("ok"));
        else ret = fo_tuple2(json_number(0), json_string("bad_size"));
    }
    free(stat_output);
    json_free(exec);
    return ret;
}

/* PoP: fo_env_unavailable_error @ tools/file_operations.py:_env_unavailable_error */
static json_t *fo_env_unavailable_error(const char *path, const char *status) {
    json_t *r = fo_read_result();
    if (strcmp(status, "env_unavailable") != 0) {
        json_set(r, "error", json_string(status));
    } else {
        char *m = fo_fmt("Terminal environment unavailable: could not stat %s "
                         "(the sandbox may still be starting or was removed). Retry shortly.", path);
        json_set(r, "error", json_string(m));
        free(m);
    }
    return r;
}

/* PoP: fo_native_path @ tools/file_operations.py:_native_path */
static json_t *fo_native_path(fo_io_t *io, const char *path) {
    if (fo_isabs(path)) return json_string(path);
    char *j = fo_join(fo_live_cwd(io), path);
    json_t *r = json_string(j);
    free(j);
    return r;
}

/* PoP: fo_detect_binary @ tools/file_operations.py:_detect_binary */
static json_t *fo_detect_binary(fo_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path));
    json_set(a, "length", json_number(1000));
    json_t *sample = fo_call(io, "sample_file_bytes", a);
    if (fo_is_raise(sample)) return sample;
    if (sample) {   /* is not None (b"" -> "" hex is present) */
        json_t *ha = json_object();
        json_set(ha, "path", json_string(path));
        json_t *hb = fo_call(io, "has_binary_extension", ha);
        if (fo_is_raise(hb)) { json_free(sample); return hb; }
        int ext_binary = fo_truthy(hb);
        json_free(hb);
        int isbin = 1;
        if (!ext_binary) {   /* short-circuit: the byte check only runs when ext is clean */
            json_t *ia = json_object();
            json_set(ia, "sample", json_copy(sample));
            json_t *ib = fo_call(io, "is_likely_binary_bytes", ia);
            if (fo_is_raise(ib)) { json_free(sample); return ib; }
            isbin = fo_truthy(ib);
            json_free(ib);
        }
        return fo_tuple2(json_bool(isbin), sample);
    }
    json_t *head = fo_head(io, path, 1000);
    if (fo_is_raise(head)) return head;
    json_t *fa = json_object();
    json_set(fa, "text", json_string(json_get_str(head, "stdout", "")));
    json_t *fr = fo_call(io, "strip_fence", fa);
    if (fo_is_raise(fr)) { json_free(head); return fr; }
    json_t *ia = json_object();
    json_set(ia, "path", json_string(path));
    json_set(ia, "content_sample", json_copy(fr));
    json_free(fr);
    json_t *ilb = fo_call(io, "is_likely_binary", ia);
    if (fo_is_raise(ilb)) { json_free(head); return ilb; }
    json_t *ret = fo_tuple2(json_bool(fo_truthy(ilb)), NULL);
    json_free(ilb);
    json_free(head);
    return ret;
}

/* PoP: fo_python_interpreter_cmd @ tools/file_operations.py:_python_interpreter_cmd */
static json_t *fo_python_interpreter_cmd(fo_io_t *io) {
    json_t *r = fo_call(io, "lsp_local_only", json_object());
    if (fo_is_raise(r)) return r;
    int local = fo_truthy(r);
    json_free(r);
    if (local) {
        json_t *a = json_object();
        json_set(a, "arg", json_string(fo_amb_str(io, "sys_executable", "/usr/bin/python3")));
        json_set(a, "translate_path", json_bool(true));
        return fo_call(io, "escape_shell_arg", a);
    }
    return json_string("python3");
}

/* PoP: fo_exec_python_snippet @ tools/file_operations.py:_exec_python_snippet */
static json_t *fo_exec_python_snippet(fo_io_t *io, const char *snippet, const json_t *py_node) {
    char *enc = fo_b64_encode((const unsigned char *)snippet, strlen(snippet));
    const char *py;
    json_t *owned_py = NULL;
    if (fo_present(py_node)) {
        py = fo_jstr(py_node);
    } else {
        owned_py = fo_python_interpreter_cmd(io);
        if (fo_is_raise(owned_py)) { free(enc); return owned_py; }
        py = fo_jstr(owned_py);
    }
    char *cmd = fo_fmt("%s -c \"import base64; exec(base64.b64decode('%s').decode())\"", py, enc);
    free(enc);
    json_free(owned_py);
    json_t *ea = json_object();
    json_set(ea, "command", json_string(cmd));
    free(cmd);
    json_t *r = fo_call(io, "exec", ea);
    if (fo_is_raise(r)) return r;
    json_t *out = fo_exec_from(r);
    json_free(r);
    return out;
}

/* string.replace(old, new) — owned result. */
static char *fo_replace(const char *s, const char *old, const char *nw) {
    size_t ol = strlen(old), nl = strlen(nw), sl = strlen(s);
    if (ol == 0) return strdup(s);
    size_t cap = sl + 1, o = 0;
    char *out = (char *)malloc(cap);
    size_t i = 0;
    while (i < sl) {
        if (i + ol <= sl && memcmp(s + i, old, ol) == 0) {
            if (o + nl + 1 > cap) { cap = (o + nl + 1) * 2; out = (char *)realloc(out, cap); }
            memcpy(out + o, nw, nl); o += nl; i += ol;
        } else {
            if (o + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
            out[o++] = s[i++];
        }
    }
    out[o] = '\0';
    return out;
}

/* Join whitespace-split tokens with no separator ("".join(s.split())). */
static char *fo_join_tokens(const char *s) {
    size_t nt = 0;
    char **tok = fo_split_ws(s, &nt);
    size_t total = 0;
    for (size_t i = 0; i < nt; i++) total += strlen(tok[i]);
    char *out = (char *)malloc(total + 1);
    size_t o = 0;
    for (size_t i = 0; i < nt; i++) { size_t l = strlen(tok[i]); memcpy(out + o, tok[i], l); o += l; }
    out[o] = '\0';
    fo_free_tokens(tok, nt);
    return out;
}

/* PoP: fo_read_exact_bytes_hex @ tools/file_operations.py:_read_exact_bytes_hex */
static json_t *fo_read_exact_bytes_hex(fo_io_t *io, const char *path, const json_t *max_bytes) {
    json_t *ret = NULL, *fr = NULL, *unavailable = NULL, *garbled = NULL, *q = NULL;
    char *body = NULL, *wc = NULL, *joined = NULL;
    json_t *more = NULL;
    int mb_present = fo_present(max_bytes);
    long long mb = mb_present ? (long long)max_bytes->num_val : 0;

    json_t *qa = json_object();
    json_set(qa, "arg", json_string(path));
    json_set(qa, "translate_path", json_bool(true));
    q = fo_call(io, "escape_shell_arg", qa);
    if (fo_is_raise(q)) { ret = q; q = NULL; goto done; }
    body = fo_bounded("od -An -v -tx1", fo_jstr(q), max_bytes);
    wc = fo_fmt("wc -c < %s", fo_jstr(q));
    json_free(q); q = NULL;
    more = json_array();
    json_append(more, json_string(wc));
    free(wc); wc = NULL;
    fr = fo_fenced_read(io, body, more);
    free(body); body = NULL;
    json_free(more); more = NULL;
    if (fo_is_raise(fr)) { ret = fr; fr = NULL; goto done; }
    {
        char *um = fo_fmt("%s: this backend has neither base64 nor od, so a byte-exact read is unavailable", path);
        unavailable = fo_exec_result(um, 1, "");
        free(um);
        char *gm = fo_fmt("%s: the backend returned a garbled byte-exact read", path);
        garbled = fo_exec_result(gm, 1, "");
        free(gm);
    }
    json_t *segs = json_get(fr, 0);
    json_t *rcn = json_get(fr, 1);
    json_t *result = json_get(fr, 2);
    if (!fo_present(segs)) {
        long long ec = (long long)json_get_num(result, "exit_code", 0);
        ret = fo_tuple2(NULL, ec != 0 ? json_copy(result) : json_copy(unavailable));
        goto done;
    }
    if (!fo_present(rcn) || (long long)rcn->num_val == 127) {
        ret = fo_tuple2(NULL, json_copy(unavailable));
        goto done;
    }
    long long rc = (long long)rcn->num_val;
    const char *payload = fo_jstr(json_get(segs, 0));
    const char *size = fo_jstr(json_get(segs, 1));
    if (rc != 0) {
        json_t *failed = fo_failed_read(io, path, payload, rc);
        if (fo_is_raise(failed)) { ret = failed; goto done; }
        ret = fo_tuple2(NULL, failed);
        goto done;
    }
    {
        json_t *fa = json_object();
        json_set(fa, "text", json_string(payload));
        json_t *sr = fo_call(io, "strip_fence", fa);
        if (fo_is_raise(sr)) { ret = sr; goto done; }
        joined = fo_join_tokens(fo_jstr(sr));
        json_free(sr);
    }
    {
        int ok;
        fo_bytes data = fo_bytes_fromhex(joined, &ok);
        free(data.data);
        if (ok && mb_present && (long long)data.len > mb) {
            json_t *tl = fo_exec_result("", 1, "");
            json_set(tl, "__too_large__", json_bool(true));
            ret = fo_tuple2(NULL, tl);
            goto done;
        }
        int matches = 0;
        if (ok) {
            json_t *ms = fo_matches_size(io, joined, size);
            if (fo_is_raise(ms)) { ret = ms; goto done; }
            matches = fo_truthy(ms);
            json_free(ms);
        }
        if (!ok || !matches) { ret = fo_tuple2(NULL, json_copy(garbled)); goto done; }
        ret = fo_tuple2(json_string(joined), NULL);
    }
done:
    json_free(fr); json_free(unavailable); json_free(garbled); json_free(q);
    json_free(more);
    free(body); free(wc); free(joined);
    return ret;
}

/* PoP: fo_read_exact_bytes @ tools/file_operations.py:_read_exact_bytes */
static json_t *fo_read_exact_bytes(fo_io_t *io, const char *path, const json_t *max_bytes) {
    json_t *ret = NULL, *too_large = NULL, *fr = NULL, *garbled = NULL, *q = NULL, *more = NULL;
    char *body = NULL, *wc = NULL;
    int mb_present = fo_present(max_bytes);
    long long mb = mb_present ? (long long)max_bytes->num_val : 0;

    if (mb_present) {
        char *cm = fo_comma(mb);
        char *m = fo_fmt("%s: file is too large (over %s bytes); read a range with offset/limit", path, cm);
        free(cm);
        too_large = fo_exec_result(m, 1, "");
        json_set(too_large, "__too_large__", json_bool(true));
        free(m);
    }
    json_t *nr = fo_call(io, "native_read_enabled", json_object());
    if (fo_is_raise(nr)) { ret = nr; goto done; }
    int native = fo_truthy(nr);
    json_free(nr);
    if (native) {
        char *full = fo_isabs(path) ? strdup(path) : fo_join(fo_live_cwd(io), path);
        json_t *oa = json_object();
        json_set(oa, "path", json_string(full));
        json_set(oa, "flags", json_number(2048));   /* O_RDONLY | O_NONBLOCK (POSIX 0o4000) */
        free(full);
        json_t *fd_r = fo_call(io, "os_open", oa);
        if (fo_is_raise(fd_r)) {
            if (fo_is_oserror(fo_raise_kind(fd_r))) { json_free(fd_r); }
            else { ret = fd_r; goto done; }
        } else if (!fd_r) {
            ret = fo_raise("TypeError", "'NoneType' object cannot be interpreted as an integer");
            goto done;
        } else {
            long long fd = (long long)fd_r->num_val;
            json_free(fd_r);
            json_t *pending = NULL, *inner_ret = NULL;
            json_t *fa = json_object();
            json_set(fa, "fd", json_number((double)fd));
            json_t *st = fo_call(io, "os_fstat", fa);
            if (fo_is_raise(st)) pending = st;
            else if (!st) pending = fo_raise("AttributeError", "'NoneType' object has no attribute 'st_mode'");
            else {
                long long mode = (long long)json_get_num(st, "st_mode", 0);
                json_free(st);
                if ((mode & 0170000) == 0100000) {   /* stat.S_ISREG */
                    json_t *fo_a = json_object();
                    json_set(fo_a, "fd", json_number((double)fd));
                    json_t *fh = fo_call(io, "fh_open", fo_a);
                    if (fo_is_raise(fh)) pending = fh;
                    else if (!fh) pending = fo_raise("AttributeError", "'NoneType' object has no attribute '__enter__'");
                    else {
                        json_free(fh);   /* file double marker; with-block exit is silent */
                        json_t *ra = json_object();
                        if (mb_present) json_set(ra, "n", json_number((double)(mb + 1)));
                        json_t *data_r = fo_call(io, "fh_read", ra);
                        if (fo_is_raise(data_r)) pending = data_r;
                        else if (!data_r) {
                            if (mb_present) pending = fo_raise("TypeError", "object of type 'NoneType' has no len()");
                            else inner_ret = fo_tuple2(NULL, NULL);
                        } else {
                            const char *hex = fo_jstr(data_r);
                            int ok;
                            fo_bytes d = fo_bytes_fromhex(hex, &ok);
                            if (mb_present && (long long)d.len > mb)
                                inner_ret = fo_tuple2(NULL, json_copy(too_large));
                            else
                                inner_ret = fo_tuple2(json_string(hex), NULL);
                            free(d.data);
                            json_free(data_r);
                        }
                    }
                }
            }
            /* finally: os.close(fd) — a close raise replaces the body outcome. */
            json_t *ca = json_object();
            json_set(ca, "fd", json_number((double)fd));
            json_t *cr = fo_call(io, "os_close", ca);
            if (fo_is_raise(cr)) {
                json_free(inner_ret); inner_ret = NULL;
                json_free(pending);
                pending = cr;
            } else json_free(cr);
            if (pending) {
                if (fo_is_oserror(fo_raise_kind(pending))) { json_free(pending); }
                else { ret = pending; goto done; }
            } else if (inner_ret) { ret = inner_ret; goto done; }
            /* non-regular file: fall through to the shell path */
        }
    }
    /* shell path */
    {
        json_t *qa = json_object();
        json_set(qa, "arg", json_string(path));
        json_set(qa, "translate_path", json_bool(true));
        q = fo_call(io, "escape_shell_arg", qa);
        if (fo_is_raise(q)) { ret = q; q = NULL; goto done; }
        body = fo_bounded("base64", fo_jstr(q), max_bytes);
        wc = fo_fmt("wc -c < %s", fo_jstr(q));
        json_free(q); q = NULL;
        more = json_array();
        json_append(more, json_string(wc));
        free(wc); wc = NULL;
        fr = fo_fenced_read(io, body, more);
        free(body); body = NULL;
        json_free(more); more = NULL;
        if (fo_is_raise(fr)) { ret = fr; fr = NULL; goto done; }
    }
    {
        char *gm = fo_fmt("%s: the backend returned a garbled byte-exact read", path);
        garbled = fo_exec_result(gm, 1, "");
        free(gm);
    }
    json_t *segs = json_get(fr, 0);
    json_t *rcn = json_get(fr, 1);
    json_t *result = json_get(fr, 2);
    if (!fo_present(segs)) {
        long long ec = (long long)json_get_num(result, "exit_code", 0);
        ret = fo_tuple2(NULL, ec != 0 ? json_copy(result) : json_copy(garbled));
        goto done;
    }
    if (!fo_present(rcn)) { ret = fo_tuple2(NULL, json_copy(garbled)); goto done; }
    long long rc = (long long)rcn->num_val;
    if (rc == 127) {
        json_t *hx = fo_read_exact_bytes_hex(io, path, max_bytes);
        if (fo_is_raise(hx)) { ret = hx; goto done; }
        json_t *failed = json_get(hx, 1);
        if (fo_is_too_large(failed)) ret = fo_tuple2(NULL, json_copy(too_large));
        else ret = json_copy(hx);
        json_free(hx);
        goto done;
    }
    const char *payload = fo_jstr(json_get(segs, 0));
    const char *size = fo_jstr(json_get(segs, 1));
    if (rc != 0) {
        json_t *failed = fo_failed_read(io, path, payload, rc);
        if (fo_is_raise(failed)) { ret = failed; goto done; }
        ret = fo_tuple2(NULL, failed);
        goto done;
    }
    {
        json_t *da = json_object();
        json_set(da, "text", json_string(payload));
        json_t *data_r = fo_call(io, "decode_base64_sample", da);
        if (fo_is_raise(data_r)) { ret = data_r; goto done; }
        if (data_r && mb_present) {
            int ok;
            fo_bytes d = fo_bytes_fromhex(fo_jstr(data_r), &ok);
            long long dl = (long long)d.len;
            free(d.data);
            if (dl > mb) {
                ret = fo_tuple2(NULL, too_large);
                too_large = NULL;
                json_free(data_r);
                goto done;
            }
        }
        int matches = 0;
        if (data_r) {
            json_t *ms = fo_matches_size(io, fo_jstr(data_r), size);
            if (fo_is_raise(ms)) { json_free(data_r); ret = ms; goto done; }
            matches = fo_truthy(ms);
            json_free(ms);
        }
        if (!data_r || !matches) ret = fo_tuple2(NULL, json_copy(garbled));
        else ret = fo_tuple2(json_copy(data_r), NULL);
        json_free(data_r);
    }
done:
    json_free(too_large); json_free(fr); json_free(garbled); json_free(q); json_free(more);
    free(body); free(wc);
    return ret;
}

/* PoP: fo_read_file_bytes @ tools/file_operations.py:read_file_bytes */
static json_t *fo_read_file_bytes(fo_io_t *io, const char *path, const json_t *max_bytes) {
    json_t *ret = NULL, *ep = NULL, *pr = NULL, *exec = NULL, *q = NULL, *sr = NULL;
    char *compact = NULL, *stripped = NULL;
    json_t *ep_a = json_object();
    json_set(ep_a, "path", json_string(path));
    ep = fo_call(io, "expand_path", ep_a);
    if (fo_is_raise(ep)) { ret = ep; ep = NULL; goto done; }
    const char *p2 = fo_jstr(ep);
    pr = fo_probe_regular_file(io, p2);
    if (fo_is_raise(pr)) { ret = pr; pr = NULL; goto done; }
    json_t *szn = json_get(pr, 0);
    long long file_size = (szn && szn->type == JSON_NUMBER) ? (long long)szn->num_val : 0;
    const char *status = fo_jstr(json_get(pr, 1));
    int mb_present = fo_present(max_bytes);
    long long mb = mb_present ? (long long)max_bytes->num_val : 0;
    if (strcmp(status, "missing") == 0) {
        ret = fo_read_result();
        char *m = fo_fmt("File not found: %s", p2);
        json_set(ret, "error", json_string(m)); free(m);
        json_set(ret, "not_found", json_bool(true));
        goto done;
    }
    if (strcmp(status, "not_regular") == 0) {
        json_t *na = json_object();
        json_set(na, "path", json_string(p2));
        json_t *nrr = fo_call(io, "not_regular_error", na);
        if (fo_is_raise(nrr)) { ret = nrr; goto done; }
        ret = fo_read_from(nrr);
        json_free(nrr);
        goto done;
    }
    if (strcmp(status, "ok") != 0 && strcmp(status, "bad_size") != 0) {
        ret = fo_env_unavailable_error(p2, status);
        goto done;
    }
    if (strcmp(status, "bad_size") == 0) {
        ret = fo_read_result();
        char *m = fo_fmt("Could not determine file size: %s", p2);
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    if (mb_present && file_size > mb) {
        ret = fo_read_result();
        json_set(ret, "file_size", json_number((double)file_size));
        char *cs = fo_comma(file_size), *cm = fo_comma(mb);
        char *m = fo_fmt("File is too large (%s bytes, limit is %s)", cs, cm);
        free(cs); free(cm);
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    {
        json_t *qa = json_object();
        json_set(qa, "arg", json_string(p2));
        json_set(qa, "translate_path", json_bool(true));
        q = fo_call(io, "escape_shell_arg", qa);
        if (fo_is_raise(q)) { ret = q; q = NULL; goto done; }
        char *cmd = fo_fmt("base64 < %s", fo_jstr(q));
        json_free(q); q = NULL;
        json_t *ea = json_object();
        json_set(ea, "command", json_string(cmd)); free(cmd);
        json_t *r = fo_call(io, "exec", ea);
        if (fo_is_raise(r)) { ret = r; goto done; }
        exec = fo_exec_from(r);
        json_free(r);
    }
    if ((long long)json_get_num(exec, "exit_code", 0) != 0) {
        ret = fo_read_result();
        char *m = fo_fmt("Failed to read binary file: %s", json_get_str(exec, "stdout", ""));
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    {
        json_t *fa = json_object();
        json_set(fa, "text", json_string(json_get_str(exec, "stdout", "")));
        sr = fo_call(io, "strip_fence", fa);
        if (fo_is_raise(sr)) { ret = sr; sr = NULL; goto done; }
        compact = fo_join_tokens(fo_jstr(sr));
        json_free(sr); sr = NULL;
    }
    if (!fo_b64_valid(compact)) {
        ret = fo_read_result();
        char *m = fo_fmt("Backend returned invalid binary data for: %s", p2);
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    ret = fo_read_result();
    json_set(ret, "base64_content", json_string(compact));
    json_set(ret, "file_size", json_number((double)file_size));
    json_set(ret, "is_binary", json_bool(true));
done:
    json_free(ep); json_free(pr); json_free(exec); json_free(q); json_free(sr);
    free(compact); free(stripped);
    return ret;
}

/* PoP: fo_reject_unencodable @ tools/file_operations.py:_reject_unencodable */
static json_t *fo_reject_unencodable(const char *path, const json_t *content_cps) {
    /* _LONE_SURROGATE_RE = [\ud800-\udc7f\udD00-\udfff]; the surrogateescape
     * range U+DC80-U+DCFF round-trips and is NOT refused. content travels as
     * a codepoint array (lone surrogates cannot cross a UTF-8 JSON transport). */
    if (!content_cps || content_cps->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_len(content_cps); i++) {
        const json_t *c = json_get(content_cps, i);
        if (!c || c->type != JSON_NUMBER) continue;
        unsigned int cp = (unsigned int)c->num_val;
        if ((cp >= 0xD800 && cp <= 0xDC7F) || (cp >= 0xDD00 && cp <= 0xDFFF)) {
            json_t *r = fo_write_result();
            char *m = fo_fmt("Refusing to write '%s': content contains a lone surrogate character "
                             "('\\u%04x') that cannot be encoded as UTF-8. The file was NOT created "
                             "or modified.", path, cp);
            json_set(r, "error", json_string(m));
            free(m);
            return r;
        }
    }
    return NULL;
}

/* PoP: fo_json_nonstandard_constant @ tools/file_operations.py:_json_nonstandard_constant */
static json_t *fo_json_nonstandard_constant(fo_io_t *io, const char *text) {
    if (!strstr(text, "NaN") && !strstr(text, "Infinity")) return NULL;
    json_t *a = json_object();
    json_set(a, "text", json_string(text));
    json_t *r = fo_call(io, "strip_bom", a);
    if (fo_is_raise(r)) {
        const char *k = fo_raise_kind(r);
        if (fo_is_valueerror(k) || strcmp(k, "RecursionError") == 0) { json_free(r); return NULL; }
        return r;
    }
    if (!r) return fo_raise("TypeError", "'NoneType' object is not subscriptable");
    const char *stripped = fo_jstr(json_get(r, 0));
    fo_jscan j = {stripped, strlen(stripped), 0, 0, 0, json_array()};
    fo_j_value(&j);
    fo_j_ws(&j);
    json_t *ret = NULL;
    if (!j.bad && j.pos == j.n && json_len(j.consts) > 0)
        ret = json_copy(json_get(j.consts, 0));
    json_free(j.consts);
    json_free(r);
    return ret;
}

/* PoP: fo_refuse_introduced_json_constant @ tools/file_operations.py:_refuse_introduced_json_constant */
static json_t *fo_refuse_introduced_json_constant(fo_io_t *io, const char *path,
                                                  const char *content, const json_t *pre_content) {
    json_t *c1 = fo_json_nonstandard_constant(io, content);
    if (fo_is_raise(c1)) return c1;
    if (!c1) return NULL;
    if (fo_present(pre_content)) {
        json_t *c2 = fo_json_nonstandard_constant(io, fo_jstr(pre_content));
        if (fo_is_raise(c2)) { json_free(c1); return c2; }
        if (c2) { json_free(c1); json_free(c2); return NULL; }
        json_free(c2);
    }
    json_t *r = fo_write_result();
    char *m = fo_fmt("Refusing to write '%s': candidate content uses %s, which is not valid JSON. "
                     "The file was NOT created or modified. Use null or a string instead and retry.",
                     path, fo_jstr(c1));
    json_free(c1);
    json_set(r, "error", json_string(m));
    free(m);
    return r;
}

/* PoP: fo_fail_closed_syntax_error @ tools/file_operations.py:_fail_closed_syntax_error */
static json_t *fo_fail_closed_syntax_error(fo_io_t *io, const char *path, const char *ext,
                                           const char *content) {
    if (!fo_amb_has_ext(io, "fail_closed_exts", ext, FO_FAIL_CLOSED_EXTS, 4)) return NULL;
    if (!fo_amb_has_ext(io, "linter_exts", ext, FO_LINTER_EXTS, 5)) return NULL;
    json_t *a = json_object();
    json_set(a, "ext", json_string(ext));
    json_set(a, "content", json_string(content));
    json_t *r = fo_call(io, "linter", a);
    if (fo_is_raise(r)) return r;
    if (!r) return fo_raise("TypeError", "cannot unpack non-iterable NoneType object");
    int ok = fo_truthy(json_get(r, 0));
    const char *err = fo_jstr(json_get(r, 1));
    if (ok || strcmp(err, "__SKIP__") == 0) { json_free(r); return NULL; }
    json_t *ret = fo_write_result();
    char *m = fo_fmt("Refusing to write '%s': candidate content fails %s syntax validation (%s). "
                     "The file was NOT created or modified. Fix the content and retry.", path, ext, err);
    json_free(r);
    json_set(ret, "error", json_string(m));
    free(m);
    return ret;
}

/* PoP: fo_verify_written_hash @ tools/file_operations.py:_verify_written_hash */
static json_t *fo_verify_written_hash(fo_io_t *io, const char *path, const char *content_hex) {
    json_t *ret = NULL, *pending = NULL, *nr = NULL, *ns = NULL, *q = NULL, *hr = NULL, *exec = NULL;
    char *disk_sha = NULL, *stripped = NULL;
    /* try: ... except Exception: pass -> (None, None). BaseException kinds pass. */
    nr = fo_call(io, "native_read_enabled", json_object());
    if (fo_is_raise(nr)) { pending = nr; nr = NULL; goto catch; }
    int native = fo_truthy(nr);
    json_free(nr); nr = NULL;
    if (native) {
        json_t *np = fo_native_path(io, path);
        json_t *na = json_object();
        json_set(na, "path", json_copy(np));
        json_free(np);
        ns = fo_call(io, "native_sha256", na);
        if (fo_is_raise(ns)) { pending = ns; ns = NULL; goto catch; }
        if (ns) { disk_sha = strdup(fo_jstr(ns)); json_free(ns); ns = NULL; }
    }
    /* hash_result = None if disk_sha else self._exec(...) */
    if (!disk_sha || !disk_sha[0]) {
        json_t *qa = json_object();
        json_set(qa, "arg", json_string(path));
        json_set(qa, "translate_path", json_bool(true));
        q = fo_call(io, "escape_shell_arg", qa);
        if (fo_is_raise(q)) { pending = q; q = NULL; goto catch; }
        char *cmd = fo_fmt("sha256sum %s 2>/dev/null", fo_jstr(q));
        json_free(q); q = NULL;
        json_t *ea = json_object();
        json_set(ea, "command", json_string(cmd)); free(cmd);
        hr = fo_call(io, "exec", ea);
        if (fo_is_raise(hr)) { pending = hr; hr = NULL; goto catch; }
        exec = fo_exec_from(hr);
        json_free(hr); hr = NULL;
        if ((long long)json_get_num(exec, "exit_code", 0) == 0) {
            stripped = fo_strip(json_get_str(exec, "stdout", ""));
            if (stripped[0]) {
                size_t nt = 0;
                char **tok = fo_split_ws(stripped, &nt);
                if (nt > 0) { free(disk_sha); disk_sha = strdup(tok[0]); }
                fo_free_tokens(tok, nt);
            }
        }
    }
    if (disk_sha && disk_sha[0]) {
        int ok;
        fo_bytes cb = fo_bytes_fromhex(content_hex, &ok);
        char *intended = fo_sha256_hex(cb.data, cb.len);
        free(cb.data);
        if (strcmp(disk_sha, intended) != 0) {
            json_t *err = fo_write_result();
            char *m = fo_fmt("Post-write verification failed for %s: on-disk content hash differs "
                             "from the intended write. The write did not persist correctly \xe2\x80\x94 "
                             "re-read the file and retry.", path);
            json_set(err, "error", json_string(m)); free(m);
            ret = fo_tuple2(json_bool(false), err);
        } else {
            ret = fo_tuple2(json_bool(true), NULL);
        }
        free(intended);
        goto done;
    }
    ret = fo_tuple2(NULL, NULL);
    goto done;
catch:
    if (pending && fo_is_exception(fo_raise_kind(pending))) {
        json_free(pending);
        ret = fo_tuple2(NULL, NULL);
    } else ret = pending;
done:
    json_free(nr); json_free(ns); json_free(q); json_free(hr); json_free(exec);
    free(disk_sha); free(stripped);
    return ret;
}

/* PoP: fo_write_file @ tools/file_operations.py:ShellFileOperations.write_file */
static json_t *fo_write_file(fo_io_t *io, const char *path, const char *content,
                             const json_t *pre_content) {
    json_t *ret = NULL, *started = NULL, *ep = NULL, *denied = NULL, *refused = NULL;
    json_t *pw = NULL, *pre2 = NULL, *nr = NULL, *hb = NULL, *aw = NULL, *wr = NULL;
    json_t *vh = NULL, *lr = NULL, *lint = NULL, *md = NULL, *lsp = NULL, *he = NULL;
    char *extl = NULL, *content2 = NULL, *dn = NULL;
    unsigned char *cbytes = NULL;
    size_t blen = 0;
    double started_v = 0;

    started = fo_call(io, "monotonic", json_object());
    if (fo_is_raise(started)) { ret = started; started = NULL; goto done; }
    started_v = started->num_val;
    json_free(started); started = NULL;

    json_t *ep_a = json_object();
    json_set(ep_a, "path", json_string(path));
    ep = fo_call(io, "expand_path", ep_a);
    if (fo_is_raise(ep)) { ret = ep; ep = NULL; goto done; }
    const char *p2 = fo_jstr(ep);

    json_t *da = json_object();
    json_set(da, "path", json_string(p2));
    json_set(da, "verb", json_string("Write"));
    json_set(da, "entry", json_bool(false));
    denied = fo_call(io, "get_write_denied_error", da);
    if (fo_is_raise(denied)) { ret = denied; denied = NULL; goto done; }
    if (fo_truthy(denied)) {
        ret = fo_write_result();
        json_set(ret, "error", json_copy(denied));
        goto done;
    }
    json_free(denied); denied = NULL;

    /* _reject_unencodable composition (content is valid UTF-8 in domain). */
    {
        size_t cn = 0;
        unsigned int *cps = fo_utf8_decode((const unsigned char *)content, strlen(content), &cn);
        json_t *cps_arr = json_array();
        for (size_t i = 0; i < cn; i++) json_append(cps_arr, json_number((double)cps[i]));
        free(cps);
        refused = fo_reject_unencodable(p2, cps_arr);
        json_free(cps_arr);
        if (refused) { ret = refused; refused = NULL; goto done; }
    }
    {
        char *root = NULL, *ext = NULL;
        fo_splitext(p2, &root, &ext);
        free(root);
        extl = fo_lower(ext);
        free(ext);
    }
    refused = fo_fail_closed_syntax_error(io, p2, extl, content);
    if (refused) { ret = refused; refused = NULL; goto done; }

    int want_pre = fo_amb_has_ext(io, "linter_exts", extl, FO_LINTER_EXTS, 5);
    if (!want_pre) {
        json_t *ha = json_object();
        json_set(ha, "ext", json_string(extl));
        he = fo_call(io, "lsp_handles_extension", ha);
        if (fo_is_raise(he)) { ret = he; he = NULL; goto done; }
        want_pre = fo_truthy(he);
        json_free(he); he = NULL;
    }
    {
        json_t *pa = json_object();
        json_set(pa, "path", json_string(p2));
        json_set(pa, "pre_content", fo_present(pre_content) ? json_copy(pre_content) : json_null());
        json_set(pa, "want_pre", json_bool(want_pre));
        pw = fo_call(io, "probe_write_target", pa);
        if (fo_is_raise(pw)) { ret = pw; pw = NULL; goto done; }
    }
    int has_bom = fo_truthy(json_get(pw, 0));
    pre2 = json_copy(json_get(pw, 1));
    const char *ending = fo_jstr(json_get(pw, 2));
    json_free(pw); pw = NULL;

    if (strcmp(extl, ".json") == 0) {
        refused = fo_refuse_introduced_json_constant(io, p2, content, pre2);
        if (refused) { ret = refused; refused = NULL; goto done; }
    }
    content2 = strdup(content);
    if (strcmp(ending, "\r\n") == 0) {
        json_t *na = json_object();
        json_set(na, "text", json_string(content2));
        json_set(na, "target", json_string("\r\n"));
        nr = fo_call(io, "normalize_line_endings", na);
        if (fo_is_raise(nr)) { ret = nr; nr = NULL; goto done; }
        free(content2);
        content2 = strdup(fo_jstr(nr));
        json_free(nr); nr = NULL;
    }
    if (has_bom) {
        json_t *ba = json_object();
        json_set(ba, "text", json_string(content2));
        hb = fo_call(io, "has_bom", ba);
        if (fo_is_raise(hb)) { ret = hb; hb = NULL; goto done; }
        int already = fo_truthy(hb);
        json_free(hb); hb = NULL;
        if (!already) {
            char *nc = fo_concat("\xef\xbb\xbf", content2);
            free(content2);
            content2 = nc;
        }
    }
    dn = fo_dirname(p2);
    int dirs_created = dn[0] ? 1 : 0;
    free(dn); dn = NULL;
    /* content.encode("utf-8", "surrogateescape") — identity on valid UTF-8. */
    {
        size_t cn = 0;
        unsigned int *cps = fo_utf8_decode((const unsigned char *)content2, strlen(content2), &cn);
        cbytes = fo_utf8_encode(cps, cn, &blen);
        free(cps);
    }
    {
        json_t *aa = json_object();
        json_set(aa, "path", json_string(p2));
        json_set(aa, "content", json_string(content2));
        aw = fo_call(io, "atomic_write", aa);
        if (fo_is_raise(aw)) { ret = aw; aw = NULL; goto done; }
        wr = fo_exec_from(aw);
        json_free(aw); aw = NULL;
    }
    if ((long long)json_get_num(wr, "exit_code", 0) != 0) {
        ret = fo_write_result();
        char *m = fo_fmt("Failed to write file: %s", json_get_str(wr, "stdout", ""));
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    {
        char *chex = fo_hex_encode(cbytes, blen);
        vh = fo_verify_written_hash(io, p2, chex);
        free(chex);
        if (fo_is_raise(vh)) { ret = vh; vh = NULL; goto done; }
    }
    if (fo_present(json_get(vh, 1))) {
        ret = json_copy(json_get(vh, 1));
        goto done;
    }
    {
        json_t *la = json_object();
        json_set(la, "path", json_string(p2));
        json_set(la, "pre_content", pre2 ? json_copy(pre2) : json_null());
        json_set(la, "post_content", json_string(content2));
        lr = fo_call(io, "check_lint_delta", la);
        if (fo_is_raise(lr)) { ret = lr; lr = NULL; goto done; }
        if (!lr) {
            ret = fo_raise("AttributeError", "'NoneType' object has no attribute 'success'");
            goto done;
        }
        lint = fo_lint_from(lr);
        json_free(lr); lr = NULL;
    }
    if (json_get_bool(lint, "success", true) || json_get_bool(lint, "skipped", false)) {
        json_t *ma = json_object();
        json_set(ma, "path", json_string(p2));
        json_set(ma, "pre_content", pre2 ? json_copy(pre2) : json_null());
        json_set(ma, "post_content", json_string(content2));
        json_set(ma, "started", json_number(started_v));
        md = fo_call(io, "maybe_lsp_diagnostics", ma);
        if (fo_is_raise(md)) { ret = md; md = NULL; goto done; }
        if (md && fo_jstr(md)[0]) lsp = json_copy(md);
        json_free(md); md = NULL;
    }
    ret = fo_write_result();
    json_set(ret, "bytes_written", json_number((double)blen));
    json_set(ret, "dirs_created", json_bool(dirs_created));
    json_set(ret, "verified", json_copy(json_get(vh, 0)));
    {
        char *sha = fo_sha256_hex(cbytes, blen);
        json_set(ret, "_content_sha256", json_string(sha));
        free(sha);
    }
    json_set(ret, "lint", fo_lint_to_dict(lint));
    json_set(ret, "lsp_diagnostics", lsp ? lsp : json_null());
    lsp = NULL;
done:
    json_free(started); json_free(ep); json_free(denied); json_free(refused);
    json_free(pw); json_free(pre2); json_free(nr); json_free(hb); json_free(aw);
    json_free(wr); json_free(vh); json_free(lr); json_free(lint); json_free(md);
    json_free(lsp); json_free(he);
    free(extl); free(content2); free(dn); free(cbytes);
    return ret;
}

/* PoP: fo_no_match_result @ tools/file_operations.py:_no_match_result */
static json_t *fo_no_match_result(fo_io_t *io, const char *path, const char *content,
                                  const char *old_string, const char *new_string,
                                  long long match_count, const json_t *error) {
    json_t *ret = NULL, *ia = NULL, *hr = NULL;
    char *err_msg = NULL;
    json_t *aa = json_object();
    json_set(aa, "content", json_string(content));
    json_set(aa, "old_string", json_string(old_string));
    json_set(aa, "new_string", json_string(new_string));
    ia = fo_call(io, "is_already_applied", aa);
    if (fo_is_raise(ia)) { ret = ia; ia = NULL; goto done; }
    if (fo_truthy(ia)) {
        ret = fo_patch_result();
        json_set(ret, "success", json_bool(true));
        json_set(ret, "no_change", json_bool(true));
        char *m = fo_fmt("File already contains the target text \xe2\x80\x94 the edit appears to be "
                         "already applied to %s. No write performed; do not re-send this patch.", path);
        json_set(ret, "note", json_string(m)); free(m);
        goto done;
    }
    json_free(ia); ia = NULL;
    if (fo_truthy(error)) err_msg = strdup(fo_jstr(error));
    else err_msg = fo_fmt("Could not find match for old_string in %s", path);
    {
        json_t *ha = json_object();
        json_set(ha, "err_msg", json_string(err_msg));
        json_set(ha, "match_count", json_number((double)match_count));
        json_set(ha, "old_string", json_string(old_string));
        json_set(ha, "content", json_string(content));
        hr = fo_call(io, "format_no_match_hint", ha);
        if (fo_is_raise(hr)) {
            if (fo_is_exception(fo_raise_kind(hr))) { json_free(hr); hr = NULL; }
            else { ret = hr; hr = NULL; goto done; }
        } else if (hr) {
            char *nm = fo_concat(err_msg, fo_jstr(hr));
            free(err_msg);
            err_msg = nm;
            json_free(hr); hr = NULL;
        } else {
            /* err_msg += None -> TypeError, swallowed by except Exception */
            json_free(hr); hr = NULL;
        }
    }
    ret = fo_patch_result();
    json_set(ret, "error", json_string(err_msg));
done:
    json_free(ia); json_free(hr);
    free(err_msg);
    return ret;
}

/* PoP: fo_verify_patch_persisted @ tools/file_operations.py:_verify_patch_persisted */
static json_t *fo_verify_patch_persisted(fo_io_t *io, const char *path, const char *new_content) {
    json_t *ret = NULL, *rb = NULL, *sb = NULL;
    char *text = NULL, *on_disk = NULL, *intended = NULL, *tmp = NULL;
    rb = fo_read_exact_bytes(io, path, NULL);
    if (fo_is_raise(rb)) { ret = rb; rb = NULL; goto done; }
    if (!fo_present(json_get(rb, 0))) {
        ret = fo_patch_result();
        char *m = fo_fmt("Post-write verification failed: could not re-read %s", path);
        json_set(ret, "error", json_string(m)); free(m);
        goto done;
    }
    {
        int ok;
        fo_bytes data = fo_bytes_fromhex(fo_jstr(json_get(rb, 0)), &ok);
        size_t cn = 0;
        unsigned int *cps = fo_utf8_decode(data.data, data.len, &cn);
        free(data.data);
        size_t tl = 0;
        unsigned char *t = fo_utf8_encode(cps, cn, &tl);
        free(cps);
        text = (char *)t;
    }
    {
        json_t *ba = json_object();
        json_set(ba, "text", json_string(text));
        sb = fo_call(io, "strip_bom", ba);
        if (fo_is_raise(sb)) { ret = sb; sb = NULL; goto done; }
    }
    const char *bomless = fo_jstr(json_get(sb, 0));
    tmp = fo_replace(bomless, "\r\n", "\n");
    on_disk = fo_replace(tmp, "\r", "\n");
    free(tmp);
    tmp = fo_replace(new_content, "\r\n", "\n");
    intended = fo_replace(tmp, "\r", "\n");
    free(tmp); tmp = NULL;   /* done: frees tmp again — keep it NULL */
    if (strcmp(on_disk, intended) != 0) {
        ret = fo_patch_result();
        char *m = fo_fmt("Post-write verification failed for %s: on-disk content differs from "
                         "intended write (wrote %zu chars, read back %zu chars after normalizing "
                         "line endings). The patch did not persist. Re-read the file and try again.",
                         path, fo_cp_len(intended), fo_cp_len(on_disk));
        json_set(ret, "error", json_string(m)); free(m);
    } else ret = NULL;
done:
    json_free(rb); json_free(sb);
    free(text); free(on_disk); free(intended); free(tmp);
    return ret;
}
