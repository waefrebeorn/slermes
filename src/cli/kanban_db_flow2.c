/* kanban_db_flow2.c — FLOW port of the hermes_cli/kanban_db.py UNCLAIMED
 * remainder AFTER kanban_adapter_flow.c (which ported 33 defs).
 *
 * PoP grep over src/ + include/ (kanban_db.py:<fn> annotations) leaves these
 * top-level defs UNCLAIMED; this file ports ALL 7 of them:
 *   pin_first_board_resolution   (contextmanager: ContextVar set/reset)
 *   _normalize_board_slug        (strip/lower/validate board slug)
 *   _resolve_project_link        (create_task project anchoring flow)
 *   _project_from_source_task    (recover Project from a worktree task)
 *   _stage_completion_artifacts  (persist + attachment-row staging flow)
 *   _discard_staged_copies       (rollback cleanup, suppress(OSError))
 *   _copy_capped                 (chunked copy with growth abort)
 * (The remaining unclaimed names are the Run/Attachment/Event dataclasses and
 * the exception ClassDefs — records, not flows; the exception classes are
 * credited via the hierarchy table below.)
 *
 * Seam model (mirrors src/cli/kanban_adapter_flow.c): every claimed I/O
 * sibling (get_task, _persist_scratch_completion_artifacts,
 * _insert_completion_attachment), every projects_db collaborator reached
 * through the in-function import (connect_closing enter/exit, get_project,
 * normalize_slug, Project), every ContextVar probe (pin_first.set /
 * pin_first.reset) and every filesystem probe (open / read / write / close /
 * stat / unlink / rmdir) goes through kd2_seam(), which appends
 * {"call": name, ...args} to io->trace (the ordered decision sequence) and
 * returns the scripted result. A raised exception is
 * {"__raise__": "<Type>", "message": "..."}; BaseException kinds (SystemExit /
 * KeyboardInterrupt / GeneratorExit) pass through every `except Exception` /
 * `suppress(ValueError)` / `suppress(OSError)` handler via the FULL builtin
 * exception-parent table below. Missing/exhausted scripts FAIL CLOSED with a
 * RuntimeError "no script for <name>" raise (harness backend). The in-function
 * `from hermes_cli import projects_db` is an "import:" seam: success retracts
 * its trace token (__untrace__), failure raises.
 *
 * Composed ported siblings stay REAL on both sides: _resolve_project_link
 * calls kd2_project_from_source_task directly (no seam).
 *
 * Deterministic subset (documented, enforced by the fixture generator):
 *  - ASCII scope for slug grammars (str().strip() honours the full Python
 *    White_Space set, .lower() is ASCII-only; Unicode lowerings that land in
 *    [a-z0-9] — Kelvin sign U+212A, dotted I — are OUT of the fixture domain).
 *  - JSON numbers lose int/float identity after the libjson parse: fixtures
 *    carry ints or NON-integral floats (repr(5.0)=="5.0" vs repr(5)=="5"
 *    would diverge in the slug ValueError message).
 *  - Paths are lexical POSIX (PurePosixPath): no filesystem is touched;
 *    '.'/empty components dropped, '..' kept, exactly-two leading slashes
 *    preserved. Path doubles expose stat/unlink/rmdir/open as seams.
 *  - Task / Project / stat results are SimpleNamespace doubles: a missing
 *    attribute raises the double-named AttributeError; a NON-dict script
 *    result is forwarded raw (attribute reads then name its real type).
 *    get_task uses `is not None` IDENTITY (an empty {} double is valid and
 *    raises on .project_id); truthiness gates elsewhere follow the live code.
 *  - Bytes travel as {"__bytes__": "<canonical padded base64>"} doubles
 *    (_copy_capped read/write chunks); the read queue must script the
 *    terminating empty chunk explicitly (exhaustion fails closed).
 *  - KANBAN_ATTACHMENT_MAX_BYTES is byte-pinned to the live 25*1024*1024
 *    (26214400) DEFAULT; a case may inject a smaller "cap" identically on
 *    both sides (the oracle seeds the module global per case) so the
 *    growth-abort branch is exercisable without 25 MB payloads. The
 *    comparison logic is transcribed verbatim.
 *  - _stage_completion_artifacts: the claimed _persist sibling's metadata
 *    mutation is modeled by the fixture pre-state (metadata already carries
 *    "_staged_artifacts"); the seam records a SNAPSHOT of metadata pre-pop.
 *    The pop itself is transcribed (on a copy — borrowed inputs are never
 *    mutated or freed).
 *  - pin_first_board_resolution is driven as set -> body -> finally-reset:
 *    "body" is null (ok) or {"raise": spec}; a reset raise masks the body
 *    raise (CPython finally semantics), a set raise skips the reset.
 *
 * Ownership: seam results and function returns are caller-owned (json_free).
 * io->trace / io->state belong to the harness. Args passed to kd2_seam are
 * consumed (copied into the trace event, then freed). Borrowed inputs are
 * never freed. A NULL json_t* input models Python None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to kanban_db.py) ──────────────────── */
#define KD2_ATTACHMENT_MAX_BYTES 26214400LL   /* 25 * 1024 * 1024 */
#define KD2_COPY_CHUNK 1048576LL              /* 1024 * 1024 */

/* ── seam dispatcher + trace ──────────────────────────────────────────────── */
typedef struct kd2_io_t {
    json_t *(*call)(struct kd2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} (harness-owned) */
    json_t *state;   /* JSON object: mutable module state (unused here) */
    void   *ctx;     /* harness backend (scripted results) */
} kd2_io_t;

static bool kd2_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *kd2_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *kd2_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    return kd2_raise(t, m);
}

/* ── FULL builtin exception hierarchy (CPython 3.14.9, verified against the
 * quarry interpreter: every builtins name subclassing BaseException, plus the
 * sqlite3 exception family). Aliases EnvironmentError/IOError/WindowsError
 * ARE OSError (is-identity verified). ExceptionGroup has the secondary parent
 * Exception. Board customs from kanban_db.py ClassDefs:
 * HallucinatedCardsError(ValueError), EmptyCompletionError(ValueError),
 * ArtifactPreservationError(RuntimeError), LiveClaimError(ValueError).
 * Used for `except Exception` / `suppress(ValueError)` / `suppress(OSError)`
 * matching so scripted BaseException kinds pass through untouched. ───────── */
typedef struct { const char *name, *parent, *parent2; } kd2_exc_entry_t;
static const kd2_exc_entry_t KD2_EXC_TABLE[] = {
    {"BaseException", NULL, NULL},
    {"SystemExit", "BaseException", NULL},
    {"KeyboardInterrupt", "BaseException", NULL},
    {"GeneratorExit", "BaseException", NULL},
    {"BaseExceptionGroup", "BaseException", NULL},
    {"Exception", "BaseException", NULL},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"StopIteration", "Exception", NULL},
    {"StopAsyncIteration", "Exception", NULL},
    {"ArithmeticError", "Exception", NULL},
    {"FloatingPointError", "ArithmeticError", NULL},
    {"OverflowError", "ArithmeticError", NULL},
    {"ZeroDivisionError", "ArithmeticError", NULL},
    {"AssertionError", "Exception", NULL},
    {"AttributeError", "Exception", NULL},
    {"BufferError", "Exception", NULL},
    {"EOFError", "Exception", NULL},
    {"ImportError", "Exception", NULL},
    {"ModuleNotFoundError", "ImportError", NULL},
    {"LookupError", "Exception", NULL},
    {"IndexError", "LookupError", NULL},
    {"KeyError", "LookupError", NULL},
    {"MemoryError", "Exception", NULL},
    {"NameError", "Exception", NULL},
    {"UnboundLocalError", "NameError", NULL},
    {"OSError", "Exception", NULL},
    {"BlockingIOError", "OSError", NULL},
    {"ChildProcessError", "OSError", NULL},
    {"ConnectionError", "OSError", NULL},
    {"BrokenPipeError", "ConnectionError", NULL},
    {"ConnectionAbortedError", "ConnectionError", NULL},
    {"ConnectionRefusedError", "ConnectionError", NULL},
    {"ConnectionResetError", "ConnectionError", NULL},
    {"FileExistsError", "OSError", NULL},
    {"FileNotFoundError", "OSError", NULL},
    {"InterruptedError", "OSError", NULL},
    {"IsADirectoryError", "OSError", NULL},
    {"NotADirectoryError", "OSError", NULL},
    {"PermissionError", "OSError", NULL},
    {"ProcessLookupError", "OSError", NULL},
    {"TimeoutError", "OSError", NULL},
    {"WindowsError", "OSError", NULL},
    {"ReferenceError", "Exception", NULL},
    {"RuntimeError", "Exception", NULL},
    {"NotImplementedError", "RuntimeError", NULL},
    {"RecursionError", "RuntimeError", NULL},
    {"PythonFinalizationError", "RuntimeError", NULL},
    {"SyntaxError", "Exception", NULL},
    {"IndentationError", "SyntaxError", NULL},
    {"TabError", "IndentationError", NULL},
    {"_IncompleteInputError", "SyntaxError", NULL},
    {"SystemError", "Exception", NULL},
    {"TypeError", "Exception", NULL},
    {"ValueError", "Exception", NULL},
    {"UnicodeError", "ValueError", NULL},
    {"UnicodeDecodeError", "UnicodeError", NULL},
    {"UnicodeEncodeError", "UnicodeError", NULL},
    {"UnicodeTranslateError", "UnicodeError", NULL},
    {"Warning", "Exception", NULL},
    {"DeprecationWarning", "Warning", NULL},
    {"PendingDeprecationWarning", "Warning", NULL},
    {"RuntimeWarning", "Warning", NULL},
    {"SyntaxWarning", "Warning", NULL},
    {"UserWarning", "Warning", NULL},
    {"FutureWarning", "Warning", NULL},
    {"ImportWarning", "Warning", NULL},
    {"UnicodeWarning", "Warning", NULL},
    {"BytesWarning", "Warning", NULL},
    {"ResourceWarning", "Warning", NULL},
    {"EncodingWarning", "Warning", NULL},
    /* sqlite3 exception family (quarry-verified parents) */
    {"Error", "Exception", NULL},
    {"DatabaseError", "Error", NULL},
    {"DataError", "DatabaseError", NULL},
    {"IntegrityError", "DatabaseError", NULL},
    {"InternalError", "DatabaseError", NULL},
    {"NotSupportedError", "DatabaseError", NULL},
    {"OperationalError", "DatabaseError", NULL},
    {"ProgrammingError", "DatabaseError", NULL},
    {"InterfaceError", "Error", NULL},
    /* board customs (kanban_db.py ClassDefs) */
    {"HallucinatedCardsError", "ValueError", NULL},
    {"EmptyCompletionError", "ValueError", NULL},
    {"ArtifactPreservationError", "RuntimeError", NULL},
    {"LiveClaimError", "ValueError", NULL},
};
static const char *kd2_exc_canon(const char *t) {
    if (strcmp(t, "EnvironmentError") == 0 || strcmp(t, "IOError") == 0 ||
        strcmp(t, "WindowsError") == 0)
        return "OSError";
    return t;
}
static const kd2_exc_entry_t *kd2_exc_find(const char *t) {
    t = kd2_exc_canon(t);
    for (size_t i = 0; i < sizeof(KD2_EXC_TABLE)/sizeof(KD2_EXC_TABLE[0]); i++)
        if (strcmp(KD2_EXC_TABLE[i].name, t) == 0) return &KD2_EXC_TABLE[i];
    return NULL;
}
/* isinstance(exc_type, parent) over the table; unknown types fail closed
 * (unknown != any builtin parent — a scripted unknown custom kind is not
 * catchable here; the fixture generator only scripts table-known kinds). */
static bool kd2_exc_isinstance(const char *type, const char *parent) {
    if (!type || !parent) return false;
    type = kd2_exc_canon(type);
    parent = kd2_exc_canon(parent);
    if (strcmp(type, parent) == 0) return true;
    const kd2_exc_entry_t *e = kd2_exc_find(type);
    int guard = 0;
    while (e && guard++ < 64) {
        if (e->parent && strcmp(e->parent, parent) == 0) return true;
        if (e->parent2 && strcmp(e->parent2, parent) == 0) return true;
        if (!e->parent) return false;
        e = kd2_exc_find(e->parent);
    }
    return false;
}

/* Record {"call":name,**args}, dispatch, consume args. An "__untrace__" result
 * (in-function import) retracts the trace token so the live silent import
 * matches; a scripted import failure also carries "__raise__". */
static json_t *kd2_seam(kd2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string(name));
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++)
            json_set(ev, args->c.keys[i], json_copy(args->c.items[i]));
    }
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, ev);
    else
        json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : json_null();
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = spec ? kd2_raise_from_spec(spec) : json_null();
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful micro helpers ────────────────────────────────────────── */
static bool kd2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool kd2_truthy(const json_t *v) {
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
static const char *kd2_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static char *kd2_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *kd2_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return kd2_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : kd2_strdup("");
}
/* Python type(v).__name__ for a JSON node (numbers: int when integral). */
static const char *kd2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == trunc(v->num_val) && fabs(v->num_val) < 9.2e18)
                       ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static json_t *kd2_attr_error(const json_t *v, const char *attr) {
    char *msg = kd2_asprintf("'%s' object has no attribute '%s'", kd2_type_name(v), attr);
    json_t *e = kd2_raise("AttributeError", msg);
    free(msg);
    return e;
}
/* SimpleNamespace-double attribute read: a dict carrier missing the key names
 * the DOUBLE; a non-dict carrier (raw script result) names its real type. */
static json_t *kd2_ns_attr(const json_t *obj, const char *key) {
    if (obj && obj->type == JSON_OBJECT) {
        if (json_has(obj, key)) return json_copy(json_obj_get(obj, key));
        char *msg = kd2_asprintf(
            "'types.SimpleNamespace' object has no attribute '%s'", key);
        json_t *e = kd2_raise("AttributeError", msg);
        free(msg);
        return e;
    }
    return kd2_attr_error(obj, key);
}
/* str equality (live `==` between a str and any other JSON scalar is False). */
static bool kd2_str_eq(const json_t *a, const char *b) {
    const char *as = kd2_str(a);
    return as && b && strcmp(as, b) == 0;
}

/* UTF-8 code point length (Python len(str)). */
static size_t kd2_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static unsigned long kd2_next_cp(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned long cp = s[0];
    int extra = 0;
    if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    for (int i = 1; i <= extra && s[i]; i++)
        cp = (cp << 6) | (s[i] & 0x3F);
    *p = s + 1 + extra;
    return cp;
}
/* Python str.isspace() set (ASCII + Unicode whitespace code points). */
static bool kd2_is_space_cp(unsigned long cp) {
    if (cp <= 0x7F)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* Python str.strip() (both ends). Caller frees. */
static char *kd2_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = kd2_next_cp(&q);
        if (!kd2_is_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = kd2_next_cp(&r);
        if (!kd2_is_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}
/* ASCII-only .lower() (fixture domain: ASCII; see header). Caller frees. */
static char *kd2_lower_ascii(const char *s) {
    char *out = kd2_strdup(s ? s : "");
    if (out)
        for (char *p = out; *p; p++)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return out;
}

/* ── string builder ───────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } kd2_sb_t;
static void kd2_sb_init(kd2_sb_t *sb) { sb->p = NULL; sb->len = 0; sb->cap = 0; }
static void kd2_sb_add(kd2_sb_t *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = (sb->len + n + 1) * 2;
        char *np = realloc(sb->p, nc);
        if (!np) return;
        sb->p = np; sb->cap = nc;
    }
    memcpy(sb->p + sb->len, s, n);
    sb->len += n;
    sb->p[sb->len] = '\0';
}
static void kd2_sb_addn(kd2_sb_t *sb, const char *s, size_t n) {
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = (sb->len + n + 1) * 2;
        char *np = realloc(sb->p, nc);
        if (!np) return;
        sb->p = np; sb->cap = nc;
    }
    memcpy(sb->p + sb->len, s, n);
    sb->len += n;
    sb->p[sb->len] = '\0';
}
static void kd2_sb_free(kd2_sb_t *sb) { free(sb->p); sb->p = NULL; sb->len = sb->cap = 0; }

/* ── Python str(number) / repr(str) / repr(number) ───────────────────────── */
/* Shortest round-trip float repr, formatted like Python repr(float). */
static void kd2_fmt_float(kd2_sb_t *sb, double v) {
    char buf[64];
    if (isnan(v)) { kd2_sb_add(sb, "NaN"); return; }
    if (isinf(v)) { kd2_sb_add(sb, v < 0 ? "-Infinity" : "Infinity"); return; }
    if (v == 0.0) { kd2_sb_add(sb, signbit(v) ? "-0.0" : "0.0"); return; }
    bool neg = v < 0;
    double a = neg ? -v : v;
    int prec;
    for (prec = 0; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*e", prec, a);
        if (strtod(buf, NULL) == a) break;
    }
    if (prec > 17) prec = 17;
    char digits[32];
    size_t nd = 0;
    const char *e = strchr(buf, 'e');
    for (const char *q = buf; q < e; q++)
        if (*q != '.') digits[nd++] = *q;
    digits[nd] = '\0';
    int exp = atoi(e + 1);
    if (neg) kd2_sb_add(sb, "-");
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t intd = (size_t)exp + 1;
            if (nd <= intd) {
                kd2_sb_add(sb, digits);
                for (size_t i = nd; i < intd; i++) kd2_sb_add(sb, "0");
                kd2_sb_add(sb, ".0");
            } else {
                char tmp[64];
                memcpy(tmp, digits, intd);
                tmp[intd] = '\0';
                kd2_sb_add(sb, tmp);
                kd2_sb_add(sb, ".");
                kd2_sb_add(sb, digits + intd);
            }
        } else {
            kd2_sb_add(sb, "0.");
            for (int i = 0; i < -exp - 1; i++) kd2_sb_add(sb, "0");
            kd2_sb_add(sb, digits);
        }
    } else {
        char tmp[8];
        tmp[0] = digits[0]; tmp[1] = '\0';
        kd2_sb_add(sb, tmp);
        if (nd > 1) {
            kd2_sb_add(sb, ".");
            kd2_sb_add(sb, digits + 1);
        }
        snprintf(buf, sizeof buf, "e%c%02d", exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
        kd2_sb_add(sb, buf);
    }
}
/* Python str() of a JSON number (integral -> int form). */
static void kd2_fmt_number(kd2_sb_t *sb, double v) {
    if (v == trunc(v) && fabs(v) < 9.2e18) {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", (long long)v);
        kd2_sb_add(sb, buf);
    } else {
        kd2_fmt_float(sb, v);
    }
}
/* Python repr() of a str (quote choice + escapes; ASCII + Latin-1
 * non-printables; non-ASCII above U+00FF passes raw — out of fixture scope).
 * Caller frees. */
static char *kd2_py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    kd2_sb_t sb;
    kd2_sb_init(&sb);
    char q[2] = { quote, '\0' };
    kd2_sb_add(&sb, q);
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        const unsigned char *start = p;
        unsigned long cp = kd2_next_cp(&p);
        if (cp == (unsigned long)quote || cp == '\\') {
            char esc[3] = { '\\', (char)cp, '\0' };
            kd2_sb_add(&sb, esc);
        } else if (cp == '\n') kd2_sb_add(&sb, "\\n");
        else if (cp == '\r') kd2_sb_add(&sb, "\\r");
        else if (cp == '\t') kd2_sb_add(&sb, "\\t");
        else if (cp < 0x20 || cp == 0x7F ||
                 (cp >= 0x80 && cp <= 0x9F) || cp == 0xA0 || cp == 0xAD) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\x%02lx", cp);
            kd2_sb_add(&sb, esc);
        } else {
            kd2_sb_addn(&sb, (const char *)start, (size_t)(p - start));
        }
    }
    kd2_sb_add(&sb, q);
    return sb.p ? sb.p : kd2_strdup("");
}
/* Python str(v) for the scalar fixture domain (str|int|float|bool).
 * Returns a malloc'd string, or NULL with *err set (containers: out of
 * domain, fail closed). NULL input models None -> "None". */
static char *kd2_py_str_node(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) return kd2_strdup("None");
    if (v->type == JSON_STRING) return kd2_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return kd2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        kd2_sb_t sb; kd2_sb_init(&sb);
        kd2_fmt_number(&sb, v->num_val);
        return sb.p ? sb.p : kd2_strdup("");
    }
    *err = kd2_raise("TypeError", "str() of a container is out of the fixture domain");
    return NULL;
}
/* Python repr(v) for the scalar fixture domain (slug ValueError message).
 * Caller frees; NULL with *err on out-of-domain containers. */
static char *kd2_py_repr_node(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) return kd2_strdup("None");
    if (v->type == JSON_STRING) return kd2_py_repr_str(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return kd2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        kd2_sb_t sb; kd2_sb_init(&sb);
        kd2_fmt_number(&sb, v->num_val);
        return sb.p ? sb.p : kd2_strdup("");
    }
    *err = kd2_raise("TypeError", "repr() of a container is out of the fixture domain");
    return NULL;
}

/* ── base64 (canonical padded) decode for bytes doubles ───────────────────── */
static int kd2_b64_val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static unsigned char *kd2_b64_decode(const char *s, size_t *out_len) {
    size_t n = strlen(s);
    if (n == 0) { *out_len = 0; unsigned char *p = malloc(1); if (p) p[0] = 0; return p; }
    if (n % 4 != 0) return NULL;
    unsigned char *out = malloc(n / 4 * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i += 4) {
        int q[4];
        for (int j = 0; j < 4; j++) {
            unsigned char c = (unsigned char)s[i + j];
            if (c == '=') {
                if (i + 4 != n || j < 2) { free(out); return NULL; }
                q[j] = 0;
            } else {
                q[j] = kd2_b64_val(c);
                if (q[j] < 0) { free(out); return NULL; }
            }
        }
        out[o++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
        if (s[i + 2] != '=') out[o++] = (unsigned char)(((q[1] & 15) << 4) | (q[2] >> 2));
        if (s[i + 3] != '=') out[o++] = (unsigned char)(((q[2] & 3) << 6) | q[3]);
    }
    *out_len = o;
    return out;
}
/* A bytes double is {"__bytes__": "<canonical b64>"} (exactly one key).
 * Returns the decoded length, or -1 with *err set (fail closed). */
static long long kd2_bytes_double_len(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type != JSON_OBJECT || v->c.count != 1 || !json_has(v, "__bytes__")) {
        *err = kd2_raise("ValueError", "invalid canonical base64 bytes double");
        return -1;
    }
    const char *b64 = json_get_str(v, "__bytes__", "");
    size_t n = 0;
    unsigned char *raw = kd2_b64_decode(b64, &n);
    if (!raw) {
        *err = kd2_raise("ValueError", "invalid canonical base64 bytes double");
        return -1;
    }
    free(raw);
    return (long long)n;
}

/* ── lexical POSIX paths (PurePosixPath) ──────────────────────────────────── */
typedef struct { char *root; char **parts; size_t n, cap; } kd2_path_t;
static void kd2_path_init(kd2_path_t *p) { p->root = kd2_strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }
static void kd2_path_push(kd2_path_t *p, const char *s, size_t n) {
    if (n == 0 || (n == 1 && s[0] == '.')) return;
    if (p->n == p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 8;
        char **np = realloc(p->parts, nc * sizeof(char *));
        if (!np) return;
        p->parts = np; p->cap = nc;
    }
    char *dup = malloc(n + 1);
    if (!dup) return;
    memcpy(dup, s, n);
    dup[n] = '\0';
    p->parts[p->n++] = dup;
}
static void kd2_path_parse(kd2_path_t *p, const char *s) {
    kd2_path_init(p);
    const char *q = s ? s : "";
    size_t slashes = 0;
    while (q[slashes] == '/') slashes++;
    if (slashes == 2) { free(p->root); p->root = kd2_strdup("//"); }
    else if (slashes >= 1) { free(p->root); p->root = kd2_strdup("/"); }
    const char *seg = q;
    for (;;) {
        const char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        kd2_path_push(p, seg, len);
        if (!slash) break;
        seg = slash + 1;
    }
}
static void kd2_path_free(kd2_path_t *p) {
    free(p->root);
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = p->cap = 0;
}
static char *kd2_path_str(const kd2_path_t *p) {
    kd2_sb_t sb;
    kd2_sb_init(&sb);
    kd2_sb_add(&sb, p->root);
    for (size_t i = 0; i < p->n; i++) {
        if (i) kd2_sb_add(&sb, "/");
        kd2_sb_add(&sb, p->parts[i]);
    }
    if (sb.len == 0) kd2_sb_add(&sb, ".");
    return sb.p ? sb.p : kd2_strdup(".");
}
/* Normalised str(Path(s)) — caller frees. */
static char *kd2_path_norm(const char *s) {
    kd2_path_t p;
    kd2_path_parse(&p, s);
    char *out = kd2_path_str(&p);
    kd2_path_free(&p);
    return out;
}
static bool kd2_path_is_absolute(const kd2_path_t *p) { return p->root[0] != '\0'; }
/* PurePosixPath.name — last component, "" when none. Borrowed pointer. */
static const char *kd2_path_name(const kd2_path_t *p) {
    return p->n ? p->parts[p->n - 1] : "";
}
/* PurePosixPath.parent — drop the last component; a component-less path is
 * its own parent (root stays; relative "." stays "."). */
static kd2_path_t kd2_path_parent(const kd2_path_t *p) {
    kd2_path_t out;
    kd2_path_init(&out);
    free(out.root);
    out.root = kd2_strdup(p->root);
    size_t n = p->n ? p->n - 1 : 0;
    for (size_t i = 0; i < n; i++)
        kd2_path_push(&out, p->parts[i], strlen(p->parts[i]));
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PoP-ported functions (all UNCLAIMED hermes_cli/kanban_db.py defs)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* PoP: kd2_normalize_board_slug @ hermes_cli/kanban_db.py:_normalize_board_slug */
json_t *kd2_normalize_board_slug(kd2_io_t *io, const json_t *slug) {
    (void)io;
    char *s = NULL;
    if (kd2_is_none(slug)) {
        s = kd2_strdup("");
    } else {
        json_t *err = NULL;
        char *raw = kd2_py_str_node(slug, &err);
        if (!raw) return err;
        char *stripped = kd2_strip(raw);
        free(raw);
        s = kd2_lower_ascii(stripped ? stripped : "");
        free(stripped);
    }
    if (!s || s[0] == '\0') {
        free(s);
        return json_null();
    }
    /* _BOARD_SLUG_RE = ^[a-z0-9][a-z0-9\-_]{0,63}$  (post-strip: full match) */
    size_t n = kd2_py_len(s);
    bool ok = n >= 1 && n <= 64;
    if (ok) {
        const unsigned char *p = (const unsigned char *)s;
        unsigned long first = kd2_next_cp(&p);
        ok = (first >= 'a' && first <= 'z') || (first >= '0' && first <= '9');
        while (ok && *p) {
            unsigned long cp = kd2_next_cp(&p);
            ok = (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') ||
                 cp == '-' || cp == '_';
        }
    }
    if (!ok) {
        json_t *err = NULL;
        char *rep = kd2_py_repr_node(slug, &err);
        if (!rep) { free(s); return err; }
        char *msg = kd2_asprintf(
            "invalid board slug %s: must be 1-64 chars, lowercase "
            "alphanumerics / hyphens / underscores, not starting with '-' or '_'",
            rep);
        free(rep);
        free(s);
        json_t *e = kd2_raise("ValueError", msg);
        free(msg);
        return e;
    }
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: kd2_pin_first_board_resolution @ hermes_cli/kanban_db.py:pin_first_board_resolution */
json_t *kd2_pin_first_board_resolution(kd2_io_t *io, const json_t *body) {
    /* token = _PIN_FIRST_BOARD_RESOLUTION.set(True) — OUTSIDE the try: a set
     * raise propagates without a reset. */
    json_t *args = json_object();
    json_set(args, "value", json_bool(true));
    json_t *token = kd2_seam(io, "pin_first.set", args);
    if (kd2_raised(token)) return token;
    /* the with-body: null = ok, {"raise": spec} = a scripted body exception
     * thrown into the generator at the yield (contextlib.__exit__). */
    json_t *body_exc = NULL;
    if (body && body->type == JSON_OBJECT) {
        json_t *spec = json_obj_get(body, "raise");
        if (spec && spec->type == JSON_OBJECT) body_exc = kd2_raise_from_spec(spec);
    }
    /* finally: _PIN_FIRST_BOARD_RESOLUTION.reset(token) — a reset raise MASKS
     * the body exception (CPython finally semantics). */
    json_t *rargs = json_object();
    json_set(rargs, "token", token);   /* consumes token */
    json_t *reset = kd2_seam(io, "pin_first.reset", rargs);
    if (kd2_raised(reset)) {
        if (body_exc) json_free(body_exc);
        return reset;
    }
    json_free(reset);
    if (body_exc) return body_exc;
    return json_null();
}

/* PoP: kd2_project_from_source_task @ hermes_cli/kanban_db.py:_project_from_source_task */
json_t *kd2_project_from_source_task(kd2_io_t *io, const char *project_id,
                                     const char *source_task_id) {
    json_t *pair_null = json_array();
    json_append(pair_null, json_null());
    json_append(pair_null, json_null());
    /* source_task = get_task(conn, source_task_id) — claimed sibling seam */
    json_t *gargs = json_object();
    json_set(gargs, "task_id", json_string(source_task_id ? source_task_id : ""));
    json_t *st = kd2_seam(io, "get_task", gargs);
    if (kd2_raised(st)) { json_free(pair_null); return st; }
    if (kd2_is_none(st)) { json_free(st); return pair_null; }
    /* `is not None` IDENTITY gate, then short-circuit attribute comparisons
     * in live order: project_id, workspace_kind, workspace_path. */
    json_t *pid_v = kd2_ns_attr(st, "project_id");
    if (kd2_raised(pid_v)) { json_free(st); json_free(pair_null); return pid_v; }
    bool match = kd2_str_eq(pid_v, project_id);
    json_free(pid_v);
    if (!match) { json_free(st); return pair_null; }
    json_t *wk_v = kd2_ns_attr(st, "workspace_kind");
    if (kd2_raised(wk_v)) { json_free(st); json_free(pair_null); return wk_v; }
    match = kd2_str_eq(wk_v, "worktree");
    json_free(wk_v);
    if (!match) { json_free(st); return pair_null; }
    json_t *wp_v = kd2_ns_attr(st, "workspace_path");
    if (kd2_raised(wp_v)) { json_free(st); json_free(pair_null); return wp_v; }
    if (!kd2_truthy(wp_v)) { json_free(wp_v); json_free(st); return pair_null; }
    const char *wps = kd2_str(wp_v);
    if (!wps) {
        /* Path(non-str) — TypeError (fixture domain: str paths) */
        char *msg = kd2_asprintf(
            "argument should be a str or an os.PathLike object where __fspath__ "
            "returns a str, not '%s'", kd2_type_name(wp_v));
        json_t *e = kd2_raise("TypeError", msg);
        free(msg);
        json_free(wp_v); json_free(st); json_free(pair_null);
        return e;
    }
    kd2_path_t sp;
    kd2_path_parse(&sp, wps);
    json_free(wp_v);
    if (!kd2_path_is_absolute(&sp)) { kd2_path_free(&sp); json_free(st); return pair_null; }
    json_t *id_v = kd2_ns_attr(st, "id");
    if (kd2_raised(id_v)) { kd2_path_free(&sp); json_free(st); json_free(pair_null); return id_v; }
    const char *id_s = kd2_str(id_v);
    bool name_ok = id_s && strcmp(kd2_path_name(&sp), id_s) == 0;
    if (!name_ok) {
        json_free(id_v); kd2_path_free(&sp); json_free(st);
        return pair_null;
    }
    kd2_path_t par = kd2_path_parent(&sp);
    bool parent_ok = strcmp(kd2_path_name(&par), ".worktrees") == 0;
    kd2_path_free(&par);
    if (!parent_ok) {
        json_free(id_v); kd2_path_free(&sp); json_free(st);
        return pair_null;
    }
    /* branch_name truthiness gate, then partition("/") */
    json_t *bn = kd2_ns_attr(st, "branch_name");
    if (kd2_raised(bn)) {
        json_free(id_v); kd2_path_free(&sp); json_free(st); json_free(pair_null);
        return bn;
    }
    json_t *slug = json_null();   /* project_slug = None */
    if (kd2_truthy(bn)) {
        const char *bns = kd2_str(bn);
        if (!bns) {
            json_t *e = kd2_attr_error(bn, "partition");
            json_free(bn); json_free(id_v); kd2_path_free(&sp); json_free(st);
            json_free(pair_null); json_free(slug);
            return e;
        }
        const char *slash = strchr(bns, '/');
        if (slash) {
            size_t plen = (size_t)(slash - bns);
            const char *leaf = slash + 1;
            char *id_prefix = kd2_asprintf("%s-", id_s);
            bool leaf_ok = strcmp(leaf, id_s) == 0 ||
                           strncmp(leaf, id_prefix, strlen(id_prefix)) == 0;
            free(id_prefix);
            if (leaf_ok) {
                char *prefix = malloc(plen + 1);
                if (prefix) { memcpy(prefix, bns, plen); prefix[plen] = '\0'; }
                json_t *nargs = json_object();
                json_set(nargs, "slug", json_string(prefix ? prefix : ""));
                free(prefix);
                json_t *r = kd2_seam(io, "normalize_slug", nargs);
                if (kd2_raised(r)) {
                    /* contextlib.suppress(ValueError) */
                    if (!kd2_exc_isinstance(json_get_str(r, "__raise__", ""), "ValueError")) {
                        json_free(bn); json_free(id_v);
                        kd2_path_free(&sp); json_free(st); json_free(pair_null);
                        json_free(slug);
                        return r;
                    }
                    json_free(r);
                } else {
                    json_free(slug);
                    slug = r;
                }
            }
        }
    }
    json_free(bn);
    if (kd2_is_none(slug)) {
        json_t *nargs = json_object();
        json_set(nargs, "slug", json_string(project_id ? project_id : ""));
        json_t *r = kd2_seam(io, "normalize_slug", nargs);
        if (kd2_raised(r)) {
            if (!kd2_exc_isinstance(json_get_str(r, "__raise__", ""), "ValueError")) {
                json_free(id_v); kd2_path_free(&sp); json_free(st);
                json_free(pair_null); json_free(slug);
                return r;
            }
            json_free(r);
        } else {
            json_free(slug);
            slug = r;
        }
    }
    if (!kd2_truthy(slug)) {
        json_free(slug); json_free(id_v); kd2_path_free(&sp); json_free(st);
        return pair_null;
    }
    const char *slug_s = kd2_str(slug);
    if (!slug_s) {
        /* normalize_slug returned a non-str truthy value: out of domain */
        json_t *e = kd2_raise("TypeError", "normalize_slug result must be a str (out of domain)");
        json_free(slug); json_free(id_v); kd2_path_free(&sp); json_free(st);
        json_free(pair_null);
        return e;
    }
    /* project_repo = str(source_path.parent.parent) */
    kd2_path_t par1 = kd2_path_parent(&sp);
    kd2_path_t par2 = kd2_path_parent(&par1);
    char *repo = kd2_path_str(&par2);
    kd2_path_free(&par1);
    kd2_path_free(&par2);
    kd2_path_free(&sp);
    /* project_obj = _pdb.Project(id=..., slug=..., name=..., created_at=0,
     *                            primary_path=project_repo) */
    json_t *pargs = json_object();
    json_set(pargs, "id", json_string(project_id ? project_id : ""));
    json_set(pargs, "slug", json_string(slug_s));
    json_set(pargs, "name", json_string(slug_s));
    json_set(pargs, "created_at", json_number(0));
    json_set(pargs, "primary_path", json_string(repo));
    json_t *pobj = kd2_seam(io, "Project", pargs);
    if (kd2_raised(pobj)) {
        free(repo); json_free(slug); json_free(id_v); json_free(st);
        json_free(pair_null);
        return pobj;
    }
    json_free(slug); json_free(id_v); json_free(st); json_free(pair_null);
    json_t *out = json_array();
    json_append(out, pobj);
    json_append(out, json_string(repo));
    free(repo);
    return out;
}

/* PoP: kd2_resolve_project_link @ hermes_cli/kanban_db.py:_resolve_project_link */
json_t *kd2_resolve_project_link(kd2_io_t *io, const json_t *project_id,
                                 const json_t *project_source_task_id,
                                 const char *workspace_kind, const json_t *workspace_path) {
    /* project_id = (str(project_id).strip() or None) if not None else None */
    char *norm = NULL;
    if (!kd2_is_none(project_id)) {
        json_t *err = NULL;
        char *raw = kd2_py_str_node(project_id, &err);
        if (!raw) return err;
        char *stripped = kd2_strip(raw);
        free(raw);
        if (stripped && stripped[0] != '\0') norm = stripped;
        else free(stripped);
    }
    const char *wk = workspace_kind ? workspace_kind : "";
    if (!norm) {
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_string(wk));
        return out;
    }
    /* from hermes_cli import projects_db as _pdb — OUTSIDE the try: an import
     * failure propagates untouched. */
    json_t *imp = kd2_seam(io, "import:hermes_cli.projects_db", json_object());
    if (kd2_raised(imp)) { free(norm); return imp; }
    json_free(imp);
    /* try: with _pdb.connect_closing() as _pconn:
     *          project_obj = _pdb.get_project(_pconn, project_id)
     * except Exception: project_obj = None */
    json_t *project_obj = json_null();
    json_t *project_repo = json_null();
    json_t *cur_exc = NULL;
    json_t *enter = kd2_seam(io, "connect_closing.enter", json_object());
    if (kd2_raised(enter)) {
        cur_exc = enter;   /* __enter__ raised: __exit__ is NOT called */
    } else {
        json_t *gargs = json_object();
        json_set(gargs, "conn", enter);   /* consumes the pconn double */
        json_set(gargs, "project_id", json_string(norm));
        json_t *gp = kd2_seam(io, "get_project", gargs);
        if (kd2_raised(gp)) {
            cur_exc = gp;
        } else {
            json_free(project_obj);
            project_obj = gp;
        }
        json_t *xargs = json_object();
        if (cur_exc) {
            json_t *e = json_object();
            json_set(e, "type", json_copy(json_obj_get(cur_exc, "__raise__")));
            json_t *m = json_obj_get(cur_exc, "message");
            json_set(e, "message", m ? json_copy(m) : json_string(""));
            json_set(xargs, "exc", e);
        } else {
            json_set(xargs, "exc", json_null());
        }
        json_t *ex = kd2_seam(io, "connect_closing.exit", xargs);
        if (kd2_raised(ex)) {
            if (cur_exc) json_free(cur_exc);
            cur_exc = ex;   /* an exit raise replaces the body exception */
        } else {
            json_free(ex);
        }
    }
    if (cur_exc) {
        if (kd2_exc_isinstance(json_get_str(cur_exc, "__raise__", ""), "Exception")) {
            json_free(cur_exc);
            json_free(project_obj);
            project_obj = json_null();
        } else {
            /* BaseException kind: passes through except Exception */
            free(norm);
            json_free(project_obj);
            json_free(project_repo);
            return cur_exc;
        }
    }
    /* if project_obj is None and project_source_task_id: fall back to the
     * composed ported sibling (REAL on both sides). */
    if (kd2_is_none(project_obj) && kd2_truthy(project_source_task_id)) {
        json_t *err = NULL;
        char *psti = kd2_py_str_node(project_source_task_id, &err);
        if (!psti) {
            free(norm); json_free(project_obj); json_free(project_repo);
            return err;
        }
        json_t *pair = kd2_project_from_source_task(io, norm, psti);
        free(psti);
        if (kd2_raised(pair)) {
            free(norm); json_free(project_obj); json_free(project_repo);
            return pair;
        }
        json_free(project_obj);
        json_free(project_repo);
        project_obj = json_copy(json_get(pair, 0));
        project_repo = json_copy(json_get(pair, 1));
        json_free(pair);
        if (!kd2_is_none(project_obj) && strcmp(wk, "scratch") == 0)
            wk = "worktree";
    }
    if (kd2_is_none(project_obj)) {
        free(norm);
        json_free(project_obj);
        json_free(project_repo);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, json_string(wk));
        return out;
    }
    /* Canonicalise + anchor the worktree under the project's primary repo. */
    json_t *primary = kd2_ns_attr(project_obj, "primary_path");
    if (kd2_raised(primary)) {
        free(norm); json_free(project_obj); json_free(project_repo);
        return primary;
    }
    if (strcmp(wk, "scratch") == 0 && kd2_truthy(primary))
        wk = "worktree";
    if (strcmp(wk, "worktree") == 0 && kd2_is_none(workspace_path) && kd2_truthy(primary)) {
        json_t *err = NULL;
        char *ps = kd2_py_str_node(primary, &err);
        if (!ps) {
            json_free(primary); free(norm);
            json_free(project_obj); json_free(project_repo);
            return err;
        }
        json_free(project_repo);
        project_repo = json_string(ps);
        free(ps);
    }
    json_free(primary);
    json_t *id_v = kd2_ns_attr(project_obj, "id");
    if (kd2_raised(id_v)) {
        free(norm); json_free(project_obj); json_free(project_repo);
        return id_v;
    }
    free(norm);
    json_t *out = json_array();
    json_append(out, id_v);
    json_append(out, project_obj);   /* consumes */
    json_append(out, project_repo);  /* consumes */
    json_append(out, json_string(wk));
    return out;
}

/* PoP: kd2_stage_completion_artifacts @ hermes_cli/kanban_db.py:_stage_completion_artifacts */
json_t *kd2_stage_completion_artifacts(kd2_io_t *io, const char *task_id,
                                       const json_t *metadata, long long now,
                                       const char *uploaded_by) {
    /* _persist_scratch_completion_artifacts(conn, task_id, metadata) — claimed
     * sibling seam; records a SNAPSHOT of metadata (pre-pop). */
    json_t *pargs = json_object();
    json_set(pargs, "task_id", json_string(task_id ? task_id : ""));
    json_set(pargs, "metadata", metadata ? json_copy(metadata) : json_null());
    json_t *pr = kd2_seam(io, "_persist_scratch_completion_artifacts", pargs);
    if (kd2_raised(pr)) return pr;
    json_free(pr);
    /* staged = [Path(p) for p in metadata.pop("_staged_artifacts", [])] —
     * transcribed on a COPY (borrowed inputs are never mutated). */
    if (!metadata || metadata->type != JSON_OBJECT) {
        json_t *e = kd2_attr_error(metadata, "pop");
        return e;
    }
    json_t *raw = json_obj_get(metadata, "_staged_artifacts");
    if (raw && raw->type != JSON_ARRAY) {
        char *msg = kd2_asprintf("'%s' object is not iterable", kd2_type_name(raw));
        json_t *e = kd2_raise("TypeError", msg);
        free(msg);
        return e;
    }
    size_t nraw = raw ? json_len(raw) : 0;   /* pop default [] */
    json_t *staged = json_array();
    for (size_t i = 0; i < nraw; i++) {
        json_t *item = json_get(raw, i);
        const char *s = kd2_str(item);
        if (!s) {
            char *msg = kd2_asprintf(
                "argument should be a str or an os.PathLike object where __fspath__ "
                "returns a str, not '%s'", kd2_type_name(item));
            json_t *e = kd2_raise("TypeError", msg);
            free(msg);
            json_free(staged);
            return e;
        }
        char *p = kd2_path_norm(s);
        json_append(staged, json_string(p));
        free(p);
    }
    for (size_t i = 0; i < json_len(staged); i++) {
        const char *p = kd2_str(json_get(staged, i));
        /* size = path.stat().st_size — stat double is a SimpleNamespace */
        json_t *sargs = json_object();
        json_set(sargs, "path", json_string(p));
        json_t *st = kd2_seam(io, "stat", sargs);
        if (kd2_raised(st)) { json_free(staged); return st; }
        json_t *size;
        if (st && st->type == JSON_OBJECT && json_has(st, "st_size")) {
            size = json_copy(json_obj_get(st, "st_size"));
        } else {
            /* the recorder materialises SimpleNamespace(**ret) for a dict and
             * an EMPTY namespace otherwise: .st_size names the double. */
            size = kd2_raise("AttributeError",
                "'types.SimpleNamespace' object has no attribute 'st_size'");
        }
        json_free(st);
        if (kd2_raised(size)) { json_free(staged); return size; }
        kd2_path_t pp;
        kd2_path_parse(&pp, p);
        const char *fname = kd2_path_name(&pp);
        json_t *iargs = json_object();
        json_set(iargs, "task_id", json_string(task_id ? task_id : ""));
        json_set(iargs, "filename", json_string(fname));
        json_set(iargs, "stored_path", json_string(p));
        json_set(iargs, "size", size);   /* consumes */
        json_set(iargs, "created_at", json_number((double)now));
        json_set(iargs, "uploaded_by", json_string(uploaded_by ? uploaded_by : "kanban_complete"));
        kd2_path_free(&pp);
        json_t *ir = kd2_seam(io, "_insert_completion_attachment", iargs);
        if (kd2_raised(ir)) { json_free(staged); return ir; }
        json_free(ir);
    }
    return staged;
}

/* PoP: kd2_discard_staged_copies @ hermes_cli/kanban_db.py:_discard_staged_copies */
json_t *kd2_discard_staged_copies(kd2_io_t *io, const json_t *copies,
                                  const char *attachment_dir) {
    if (!copies || copies->type != JSON_ARRAY) {
        char *msg = kd2_asprintf("'%s' object is not iterable", kd2_type_name(copies));
        json_t *e = kd2_raise("TypeError", msg);
        free(msg);
        return e;
    }
    {
        for (size_t i = 0; i < json_len(copies); i++) {
            json_t *item = json_get(copies, i);
            const char *s = kd2_str(item);
            if (!s) {
                char *msg = kd2_asprintf(
                    "argument should be a str or an os.PathLike object where __fspath__ "
                    "returns a str, not <class '%s'>", kd2_type_name(item));
                json_t *e = kd2_raise("TypeError", msg);
                free(msg);
                return e;
            }
            char *p = kd2_path_norm(s);
            json_t *args = json_object();
            json_set(args, "path", json_string(p));
            json_set(args, "missing_ok", json_bool(true));
            free(p);
            json_t *r = kd2_seam(io, "unlink", args);
            if (kd2_raised(r)) {
                /* contextlib.suppress(OSError) — subclasses included */
                if (!kd2_exc_isinstance(json_get_str(r, "__raise__", ""), "OSError"))
                    return r;
            }
            json_free(r);
        }
    }
    char *ad = kd2_path_norm(attachment_dir ? attachment_dir : "");
    json_t *args = json_object();
    json_set(args, "path", json_string(ad));
    free(ad);
    json_t *r = kd2_seam(io, "rmdir", args);
    if (kd2_raised(r)) {
        if (!kd2_exc_isinstance(json_get_str(r, "__raise__", ""), "OSError"))
            return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: kd2_copy_capped @ hermes_cli/kanban_db.py:_copy_capped */
json_t *kd2_copy_capped(kd2_io_t *io, const char *src, const char *dest,
                        const char *artifact, long long cap) {
    char *src_p = kd2_path_norm(src ? src : "");
    char *dest_p = kd2_path_norm(dest ? dest : "");
    /* with src.open("rb") as source_file, dest.open("xb") as destination_file:
     * — the open seams return the file doubles; __enter__ is untraced. */
    json_t *oargs = json_object();
    json_set(oargs, "path", json_string(src_p));
    json_set(oargs, "mode", json_string("rb"));
    json_t *src_open = kd2_seam(io, "open", oargs);
    if (kd2_raised(src_open)) {
        free(src_p); free(dest_p);
        return src_open;   /* nothing entered: no close */
    }
    json_free(src_open);
    oargs = json_object();
    json_set(oargs, "path", json_string(dest_p));
    json_set(oargs, "mode", json_string("xb"));
    json_t *dest_open = kd2_seam(io, "open", oargs);
    if (kd2_raised(dest_open)) {
        /* src already entered: its __exit__ runs with the open exception */
        json_t *cargs = json_object();
        json_set(cargs, "path", json_string(src_p));
        json_t *cl = kd2_seam(io, "close", cargs);
        free(src_p); free(dest_p);
        if (kd2_raised(cl)) { json_free(dest_open); return cl; }
        json_free(cl);
        return dest_open;
    }
    json_free(dest_open);
    /* copied = 0; while chunk := source_file.read(1024*1024): ... */
    long long copied = 0;
    json_t *exc = NULL;
    for (;;) {
        json_t *rargs = json_object();
        json_set(rargs, "path", json_string(src_p));
        json_set(rargs, "size", json_number((double)KD2_COPY_CHUNK));
        json_t *rd = kd2_seam(io, "read", rargs);
        if (kd2_raised(rd)) { exc = rd; break; }
        json_t *berr = NULL;
        long long n = kd2_bytes_double_len(rd, &berr);
        if (n < 0) { json_free(rd); exc = berr; break; }
        if (n == 0) { json_free(rd); break; }   /* empty chunk: loop ends */
        copied += n;
        if (copied > cap) {
            char *msg = kd2_asprintf(
                "declared scratch artifact grew beyond the size limit: %s",
                artifact ? artifact : "");
            exc = kd2_raise("ArtifactPreservationError", msg);
            free(msg);
            json_free(rd);
            break;
        }
        json_t *wargs = json_object();
        json_set(wargs, "path", json_string(dest_p));
        json_set(wargs, "data", rd);   /* consumes the bytes double */
        json_t *wr = kd2_seam(io, "write", wargs);
        if (kd2_raised(wr)) { exc = wr; break; }
        json_free(wr);
    }
    /* with-block exit: REVERSE order (dest first, then src); a close raise
     * replaces the in-flight exception, and the outer close still runs. */
    json_t *cargs = json_object();
    json_set(cargs, "path", json_string(dest_p));
    json_t *cl = kd2_seam(io, "close", cargs);
    if (kd2_raised(cl)) { if (exc) json_free(exc); exc = cl; }
    else json_free(cl);
    cargs = json_object();
    json_set(cargs, "path", json_string(src_p));
    cl = kd2_seam(io, "close", cargs);
    if (kd2_raised(cl)) { if (exc) json_free(exc); exc = cl; }
    else json_free(cl);
    free(src_p); free(dest_p);
    if (exc) return exc;
    return json_null();
}