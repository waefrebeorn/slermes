/* kanban_adapter_flow.c — FLOW + leaf port of the kanban adapter's UNCLAIMED
 * remainder.
 *
 * MODULE RESOLUTION: the task names "hermes_cli/kanban_adapter.py", which does
 * not exist in the quarry. The legacy adapter object (src/tools/kanban_adapter.c)
 * declares its semantic source of truth as hermes_cli/kanban_db.py ("PoP:
 * faithful port. Semantic source of truth = hermes_cli/kanban_db.py"), and
 * scripts/pyport_map.json maps that object to kanban_db.py. The UNCLAIMED
 * remainder (PoP grep over src/include/tests) is therefore the 39 unclaimed
 * top-level defs of hermes_cli/kanban_db.py; this file ports 33 of them and
 * PoP-credits the REAL defining module (the scanner only credits the defining
 * module; a phantom path would credit nothing).
 *
 * Ported here (all previously UNCLAIMED by PoP):
 *   leaves:  _lossy_text _row_get _json_or _json_dict _env_int _first_line
 *            _opt_int _json_or_null _substantive_text _nonblank_str
 *            _cleaned_artifact_paths _retention_seconds _slug_or_default
 *            _require_slug
 *   flows:   _host_prefix _board_meta_for _hook_profile_name _fire_task_hook
 *            _explicit_board_slug _explicit_board_intent_pinned _board_path
 *            _link _linked_ids _require_task _task_rows _latest_event
 *            _counts_by_assignee current_run_started_ats _end_or_synthesize_run
 *            _gate_created_cards _flag_phantom_prose_refs _git_out
 *            _project_branch_name
 *
 * Seam model (mirrors port_kanban_db_flow.c / kanban_dashboard_flow.c): every
 * conn.execute / cursor fetch / write_txn boundary / claimed I/O sibling
 * (_normalize_board_slug, get_current_board, read_board_metadata, _claimer_id,
 * kanban_home, board_dir, _fire_kanban_lifecycle_hook, _end_run,
 * _synthesize_ended_run, _verify_created_cards, _append_event,
 * _scan_prose_for_phantom_ids, subprocess.run) and every environment probe
 * (os.environ.get, ContextVar.get, Path.expanduser, in-function imports) goes
 * through ka_seam(), which appends {"call": name, ...args} to io->trace (the
 * ordered decision sequence) and returns the scripted result. A raised
 * exception is {"__raise__": "<Type>", "message": "..."}; BaseException kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit) pass through every
 * `except Exception` / `except ValueError` handler via the FULL builtin
 * exception-parent table below. Missing/exhausted scripts FAIL CLOSED with a
 * RuntimeError "no script for <name>" raise (harness backend).
 *
 * Deterministic subset (documented, enforced by the fixture generator):
 *  - ASCII scope for int()/strip()/splitlines() grammars plus the common
 *    Unicode whitespace / line-separator code points (U+0085/2028/2029).
 *  - _lossy_text bytes travel as {"__bytes__": "<canonical padded base64>"};
 *    the UTF-8 replace decoder implements CPython's maximal-subpart rule.
 *  - _json_or pre-validates the STRICT Python json.loads grammar before the
 *    lenient libjson parse (no leading zeros / bare '-' / '1.' / invalid
 *    escapes / raw control chars / trailing commas; ws = space tab CR LF).
 *    NaN/Infinity, lone-surrogate escapes, >2^53 ints and depth>32 are outside
 *    the fixture domain.
 *  - _json_or_null: json.dumps(ensure_ascii=False) with default separators
 *    (", ", ": "); JSON numbers lose int/float identity after parsing, so
 *    fixtures carry ints or NON-integral floats (json.dumps(1.0) would emit
 *    "1.0" where the parsed-int C side emits "1").
 *  - Paths are lexical POSIX (PurePosixPath): Path.expanduser is a seam, no
 *    filesystem is touched; '..' is kept, '.' and empty components dropped,
 *    exactly-two leading slashes preserved.
 *  - sqlite3.Row is a dict double (KeyError on a missing column); cursor
 *    iteration records one "iter" event then one "next" event per YIELDED row
 *    (the final StopIteration __next__ is untraced on both sides).
 *  - _fire_task_hook task doubles are SimpleNamespace: a present task without
 *    an assignee key raises the double-named AttributeError; **fields keys
 *    must not collide with event/task_id/board/assignee/run_id.
 *  - _verify_created_cards scripts are 2-element arrays or null (tuple
 *    unpack); _git_out subprocess.run scripts carry returncode+stdout.
 *
 * Ownership: seam results and function returns are caller-owned (json_free).
 * io->trace / io->state belong to the harness. Args passed to ka_seam are
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
#define KA_DEFAULT_BOARD "default"

/* SQL fragments — byte-pinned; gen_kanban_adapter_flow_cases.py --check-sql
 * verifies every macro against the live AST (static concat / f-string parts). */
#define KA_SQL_LINK \
    "INSERT OR IGNORE INTO task_links (parent_id, child_id) VALUES (?, ?)"
#define KA_SQL_REQUIRE_TASK "SELECT 1 FROM tasks WHERE id = ?"
#define KA_SQL_LATEST_BASE \
    "SELECT payload FROM task_events WHERE task_id = ? AND kind = ?"
#define KA_SQL_LATEST_RUN " AND run_id = ?"
#define KA_SQL_LATEST_ORDER " ORDER BY id DESC LIMIT 1"
#define KA_SQL_COUNTS \
    "SELECT assignee, status, COUNT(*) AS n FROM tasks " \
    "WHERE status != 'archived' AND assignee IS NOT NULL " \
    "GROUP BY assignee, status"
#define KA_SQL_RUN_STARTED_PRE \
    "SELECT t.id AS task_id, r.started_at AS started_at FROM tasks t " \
    "JOIN task_runs r ON r.id = t.current_run_id " \
    "WHERE t.id IN ("
#define KA_SQL_RUN_STARTED_POST ")"
#define KA_SQL_LINKED_PRE "SELECT "
#define KA_SQL_LINKED_MID " FROM task_links WHERE "
#define KA_SQL_LINKED_POST " = ? ORDER BY "
#define KA_SQL_TASK_ROWS_PRE "SELECT * FROM "
#define KA_SQL_TASK_ROWS_MID " WHERE task_id = ? ORDER BY "

/* ── seam dispatcher + trace ──────────────────────────────────────────────── */
typedef struct ka_io_t {
    json_t *(*call)(struct ka_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} (harness-owned) */
    json_t *state;   /* JSON object: mutable module state (unused here) */
    void   *ctx;     /* harness backend (scripted results) */
} ka_io_t;

static bool ka_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *ka_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *ka_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    return ka_raise(t, m);
}

/* ── FULL builtin exception hierarchy (CPython 3.14, verified against the
 * quarry interpreter: every builtins name subclassing BaseException).
 * Aliases EnvironmentError/IOError ARE OSError (canonicalised below);
 * ExceptionGroup has the secondary parent Exception. Custom board errors:
 * HallucinatedCardsError(ValueError), ArtifactPreservationError(RuntimeError).
 * Used for `except Exception` / `except ValueError` matching so scripted
 * BaseException kinds pass through untouched. ─────────────────────────────── */
typedef struct { const char *name, *parent, *parent2; } ka_exc_entry_t;
static const ka_exc_entry_t KA_EXC_TABLE[] = {
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
    /* board customs (kanban_db.py ClassDefs) */
    {"HallucinatedCardsError", "ValueError", NULL},
    {"EmptyCompletionError", "ValueError", NULL},
    {"ArtifactPreservationError", "RuntimeError", NULL},
    {"LiveClaimError", "RuntimeError", NULL},
};
static const char *ka_exc_canon(const char *t) {
    /* EnvironmentError / IOError / WindowsError are aliases OF OSError
     * (same class object on this CPython build — verified is-identity). */
    if (strcmp(t, "EnvironmentError") == 0 || strcmp(t, "IOError") == 0 ||
        strcmp(t, "WindowsError") == 0)
        return "OSError";
    return t;
}
static const ka_exc_entry_t *ka_exc_find(const char *t) {
    t = ka_exc_canon(t);
    for (size_t i = 0; i < sizeof(KA_EXC_TABLE)/sizeof(KA_EXC_TABLE[0]); i++)
        if (strcmp(KA_EXC_TABLE[i].name, t) == 0) return &KA_EXC_TABLE[i];
    return NULL;
}
/* isinstance(exc_type, parent) over the table; unknown types are opaque
 * Exception subclasses ONLY when named like one — fail closed: unknown != any
 * builtin parent (a scripted unknown custom kind is not catchable here). */
static bool ka_exc_isinstance(const char *type, const char *parent) {
    if (!type || !parent) return false;
    type = ka_exc_canon(type);
    parent = ka_exc_canon(parent);
    if (strcmp(type, parent) == 0) return true;
    const ka_exc_entry_t *e = ka_exc_find(type);
    int guard = 0;
    while (e && guard++ < 64) {
        if (e->parent && strcmp(e->parent, parent) == 0) return true;
        if (e->parent2 && strcmp(e->parent2, parent) == 0) return true;
        if (!e->parent) return false;
        e = ka_exc_find(e->parent);
    }
    return false;
}

/* Record {"call":name,**args}, dispatch, consume args. An "__untrace__" result
 * (in-function import) retracts the trace token so the live silent import
 * matches; a scripted import failure also carries "__raise__". */
static json_t *ka_seam(ka_io_t *io, const char *name, json_t *args) {
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
        json_t *out = spec ? ka_raise_from_spec(spec) : json_null();
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful micro helpers ────────────────────────────────────────── */
static bool ka_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ka_truthy(const json_t *v) {
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
static const char *ka_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static char *ka_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *ka_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return ka_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : ka_strdup("");
}
/* Python type(v).__name__ for a JSON node (numbers: int when integral). */
static const char *ka_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
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
static json_t *ka_attr_error(const json_t *v, const char *attr) {
    char *msg = ka_asprintf("'%s' object has no attribute '%s'", ka_type_name(v), attr);
    json_t *e = ka_raise("AttributeError", msg);
    free(msg);
    return e;
}

/* UTF-8 code point length (Python len(str)). */
static size_t ka_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* Decode one UTF-8 code point at *p; advances *p. */
static unsigned long ka_next_cp(const unsigned char **p) {
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
static bool ka_is_space_cp(unsigned long cp) {
    if (cp <= 0x7F)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* Python str.strip() (both ends). Caller frees. */
static char *ka_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = ka_next_cp(&q);
        if (!ka_is_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = ka_next_cp(&r);
        if (!ka_is_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}
/* int()'s whitespace set is NARROWER than str.strip(): it does NOT include
 * \x1c-\x1f (int('1\x1c') raises ValueError while '1\x1c'.strip()=='1').
 * Verified against the quarry interpreter for every Unicode ws code point. */
static bool ka_is_int_space_cp(unsigned long cp) {
    if (cp >= 0x1C && cp <= 0x1F) return false;
    return ka_is_space_cp(cp);
}
static char *ka_int_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = ka_next_cp(&q);
        if (!ka_is_int_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = ka_next_cp(&r);
        if (!ka_is_int_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}
/* Python str.splitlines() boundary width at p (0 when none). */
static size_t ka_line_sep(const unsigned char *p) {
    if (p[0] == '\r') return (p[1] == '\n') ? 2 : 1;
    if (p[0] == '\n' || p[0] == '\v' || p[0] == '\f' ||
        p[0] == 0x1C || p[0] == 0x1D || p[0] == 0x1E) return 1;
    if (p[0] == 0xC2 && p[1] == 0x85) return 2;
    if (p[0] == 0xE2 && p[1] == 0x80 && (p[2] == 0xA8 || p[2] == 0xA9)) return 3;
    return 0;
}

/* ── string builder ───────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } ka_sb_t;
static void ka_sb_init(ka_sb_t *sb) { sb->p = NULL; sb->len = 0; sb->cap = 0; }
static void ka_sb_add(ka_sb_t *sb, const char *s) {
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
static void ka_sb_addn(ka_sb_t *sb, const char *s, size_t n) {
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
static void ka_sb_free(ka_sb_t *sb) { free(sb->p); sb->p = NULL; sb->len = sb->cap = 0; }

/* ── Python repr(str) / str(number) / json.dumps(ensure_ascii=False) ──────── */
/* Shortest round-trip float repr, formatted like Python repr(float). */
static void ka_fmt_float(ka_sb_t *sb, double v) {
    char buf[64];
    if (isnan(v)) { ka_sb_add(sb, "NaN"); return; }
    if (isinf(v)) { ka_sb_add(sb, v < 0 ? "-Infinity" : "Infinity"); return; }
    if (v == 0.0) { ka_sb_add(sb, signbit(v) ? "-0.0" : "0.0"); return; }
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
    if (neg) ka_sb_add(sb, "-");
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t intd = (size_t)exp + 1;
            if (nd <= intd) {
                ka_sb_add(sb, digits);
                for (size_t i = nd; i < intd; i++) ka_sb_add(sb, "0");
                ka_sb_add(sb, ".0");
            } else {
                char tmp[64];
                memcpy(tmp, digits, intd);
                tmp[intd] = '\0';
                ka_sb_add(sb, tmp);
                ka_sb_add(sb, ".");
                ka_sb_add(sb, digits + intd);
            }
        } else {
            ka_sb_add(sb, "0.");
            for (int i = 0; i < -exp - 1; i++) ka_sb_add(sb, "0");
            ka_sb_add(sb, digits);
        }
    } else {
        char tmp[8];
        tmp[0] = digits[0]; tmp[1] = '\0';
        ka_sb_add(sb, tmp);
        if (nd > 1) {
            ka_sb_add(sb, ".");
            ka_sb_add(sb, digits + 1);
        }
        snprintf(buf, sizeof buf, "e%c%02d", exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
        ka_sb_add(sb, buf);
    }
}
/* Python str() of a JSON number (integral -> int form). */
static void ka_fmt_number(ka_sb_t *sb, double v) {
    if (v == trunc(v) && fabs(v) < 9.2e18) {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", (long long)v);
        ka_sb_add(sb, buf);
    } else {
        ka_fmt_float(sb, v);
    }
}
/* Python repr() of a str (quote choice + escapes). Covers ASCII plus the
 * Latin-1 non-printables (C1 controls 0x80-0x9F, NBSP 0xA0, soft hyphen
 * 0xAD); non-ASCII above U+00FF passes raw (out of fixture scope for int()
 * error messages). Caller frees. */
static char *ka_py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    ka_sb_t sb;
    ka_sb_init(&sb);
    char q[2] = { quote, '\0' };
    ka_sb_add(&sb, q);
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        const unsigned char *start = p;
        unsigned long cp = ka_next_cp(&p);
        if (cp == (unsigned long)quote || cp == '\\') {
            char esc[3] = { '\\', (char)cp, '\0' };
            ka_sb_add(&sb, esc);
        } else if (cp == '\n') ka_sb_add(&sb, "\\n");
        else if (cp == '\r') ka_sb_add(&sb, "\\r");
        else if (cp == '\t') ka_sb_add(&sb, "\\t");
        else if (cp < 0x20 || cp == 0x7F ||
                 (cp >= 0x80 && cp <= 0x9F) || cp == 0xA0 || cp == 0xAD) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\x%02lx", cp);
            ka_sb_add(&sb, esc);
        } else {
            ka_sb_addn(&sb, (const char *)start, (size_t)(p - start));
        }
    }
    ka_sb_add(&sb, q);
    return sb.p ? sb.p : ka_strdup("");
}
/* json.dumps(obj, ensure_ascii=False) — default separators (", ", ": "),
 * insertion-ordered keys, integral doubles as ints (fixture domain: JSON
 * numbers are live ints; non-integral floats use repr). Caller frees. */
static void ka_dumps_rec(ka_sb_t *sb, const json_t *v) {
    if (!v || v->type == JSON_NULL) { ka_sb_add(sb, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: ka_sb_add(sb, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: ka_fmt_number(sb, v->num_val); break;
        case JSON_STRING: {
            ka_sb_add(sb, "\"");
            for (const unsigned char *p = (const unsigned char *)(v->str_val ? v->str_val : ""); *p; p++) {
                unsigned char c = *p;
                if (c == '"') ka_sb_add(sb, "\\\"");
                else if (c == '\\') ka_sb_add(sb, "\\\\");
                else if (c == '\n') ka_sb_add(sb, "\\n");
                else if (c == '\r') ka_sb_add(sb, "\\r");
                else if (c == '\t') ka_sb_add(sb, "\\t");
                else if (c == '\b') ka_sb_add(sb, "\\b");
                else if (c == '\f') ka_sb_add(sb, "\\f");
                else if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", c);
                    ka_sb_add(sb, esc);
                } else {
                    char ch[2] = { (char)c, '\0' };
                    ka_sb_add(sb, ch);
                }
            }
            ka_sb_add(sb, "\"");
            break;
        }
        case JSON_ARRAY:
            ka_sb_add(sb, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ka_sb_add(sb, ", ");
                ka_dumps_rec(sb, v->c.items[i]);
            }
            ka_sb_add(sb, "]");
            break;
        case JSON_OBJECT:
            ka_sb_add(sb, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) ka_sb_add(sb, ", ");
                json_t *k = json_string(v->c.keys[i]);
                ka_dumps_rec(sb, k);
                json_free(k);
                ka_sb_add(sb, ": ");
                ka_dumps_rec(sb, v->c.items[i]);
            }
            ka_sb_add(sb, "}");
            break;
        default: ka_sb_add(sb, "null"); break;
    }
}
static char *ka_py_dumps(const json_t *v) {
    ka_sb_t sb;
    ka_sb_init(&sb);
    ka_dumps_rec(&sb, v);
    return sb.p ? sb.p : ka_strdup("null");
}

/* ── strict Python json.loads grammar pre-validator ─────────────────────────
 * libjson json_parse is LENIENT (leading zeros, bare '-', invalid escapes,
 * raw control chars, any byte <= 0x20 as ws). Python json.loads is strict;
 * validate first so the accept/reject domains agree. NaN/Infinity and lone
 * surrogates are outside the fixture domain (rejected here). */
typedef struct { const unsigned char *p, *end; int depth; } ka_jv_t;
static bool ka_jv_ws(ka_jv_t *v) {
    while (v->p < v->end && (*v->p == ' ' || *v->p == '\t' || *v->p == '\n' || *v->p == '\r'))
        v->p++;
    return true;
}
static bool ka_jv_value(ka_jv_t *v);
static bool ka_jv_string(ka_jv_t *v) {
    if (v->p >= v->end || *v->p != '"') return false;
    v->p++;
    while (v->p < v->end) {
        unsigned char c = *v->p;
        if (c == '"') { v->p++; return true; }
        if (c < 0x20) return false;           /* raw control char: strict */
        if (c == '\\') {
            v->p++;
            if (v->p >= v->end) return false;
            unsigned char e = *v->p;
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' ||
                e == 'n' || e == 'r' || e == 't') { v->p++; continue; }
            if (e == 'u') {
                v->p++;
                if (v->end - v->p < 4) return false;
                unsigned long cp = 0;
                for (int i = 0; i < 4; i++) {
                    unsigned char h = v->p[i];
                    int d;
                    if (h >= '0' && h <= '9') d = h - '0';
                    else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                    else return false;
                    cp = (cp << 4) | (unsigned long)d;
                }
                v->p += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    /* high surrogate: require a paired low escape */
                    if (v->end - v->p < 6 || v->p[0] != '\\' || v->p[1] != 'u')
                        return false;
                    unsigned long lo = 0;
                    for (int i = 0; i < 4; i++) {
                        unsigned char h = v->p[2 + i];
                        int d;
                        if (h >= '0' && h <= '9') d = h - '0';
                        else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                        else return false;
                        lo = (lo << 4) | (unsigned long)d;
                    }
                    if (lo < 0xDC00 || lo > 0xDFFF) return false;
                    v->p += 6;
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;             /* lone low surrogate: out of domain */
                }
                continue;
            }
            return false;                     /* invalid escape (\q etc.) */
        }
        v->p++;
    }
    return false;                             /* unterminated */
}
static bool ka_jv_number(ka_jv_t *v) {
    const unsigned char *s = v->p;
    if (v->p < v->end && *v->p == '-') v->p++;
    if (v->p >= v->end) return false;
    if (*v->p == '0') {
        v->p++;
        if (v->p < v->end && *v->p >= '0' && *v->p <= '9') return false; /* 01 */
    } else if (*v->p >= '1' && *v->p <= '9') {
        while (v->p < v->end && *v->p >= '0' && *v->p <= '9') v->p++;
    } else {
        return false;
    }
    if (v->p < v->end && *v->p == '.') {
        v->p++;
        if (v->p >= v->end || *v->p < '0' || *v->p > '9') return false; /* 1. */
        while (v->p < v->end && *v->p >= '0' && *v->p <= '9') v->p++;
    }
    if (v->p < v->end && (*v->p == 'e' || *v->p == 'E')) {
        v->p++;
        if (v->p < v->end && (*v->p == '+' || *v->p == '-')) v->p++;
        if (v->p >= v->end || *v->p < '0' || *v->p > '9') return false; /* 1e */
        while (v->p < v->end && *v->p >= '0' && *v->p <= '9') v->p++;
    }
    return v->p > s;
}
static bool ka_jv_value(ka_jv_t *v) {
    if (v->depth > 32) return false;
    if (v->p >= v->end) return false;
    unsigned char c = *v->p;
    if (c == '"') return ka_jv_string(v);
    if (c == '-' || (c >= '0' && c <= '9')) return ka_jv_number(v);
    if (c == '{') {
        v->p++; v->depth++;
        ka_jv_ws(v);
        if (v->p < v->end && *v->p == '}') { v->p++; v->depth--; return true; }
        for (;;) {
            ka_jv_ws(v);
            if (!ka_jv_string(v)) return false;   /* keys must be strings */
            ka_jv_ws(v);
            if (v->p >= v->end || *v->p != ':') return false;
            v->p++;
            ka_jv_ws(v);
            if (!ka_jv_value(v)) return false;
            ka_jv_ws(v);
            if (v->p >= v->end) return false;
            if (*v->p == '}') { v->p++; v->depth--; return true; }
            if (*v->p != ',') return false;
            v->p++;
        }
    }
    if (c == '[') {
        v->p++; v->depth++;
        ka_jv_ws(v);
        if (v->p < v->end && *v->p == ']') { v->p++; v->depth--; return true; }
        for (;;) {
            ka_jv_ws(v);
            if (!ka_jv_value(v)) return false;
            ka_jv_ws(v);
            if (v->p >= v->end) return false;
            if (*v->p == ']') { v->p++; v->depth--; return true; }
            if (*v->p != ',') return false;
            v->p++;
        }
    }
    if (v->end - v->p >= 4 && memcmp(v->p, "true", 4) == 0) { v->p += 4; return true; }
    if (v->end - v->p >= 5 && memcmp(v->p, "false", 5) == 0) { v->p += 5; return true; }
    if (v->end - v->p >= 4 && memcmp(v->p, "null", 4) == 0) { v->p += 4; return true; }
    return false;
}
static bool ka_json_strict_valid(const char *s) {
    ka_jv_t v;
    v.p = (const unsigned char *)s;
    v.end = v.p + strlen(s);
    v.depth = 0;
    ka_jv_ws(&v);
    if (!ka_jv_value(&v)) return false;
    ka_jv_ws(&v);
    return v.p == v.end;
}

/* ── base64 (canonical padded) decode for the _lossy_text bytes double ────── */
static int ka_b64_val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/* Decode canonical padded base64; returns malloc'd bytes + *out_len, or NULL
 * on any malformed input (fail closed). */
static unsigned char *ka_b64_decode(const char *s, size_t *out_len) {
    size_t n = strlen(s);
    if (n == 0) { *out_len = 0; unsigned char *p = malloc(1); if (p) p[0] = 0; return p; }
    if (n % 4 != 0) return NULL;
    size_t pad = 0;
    if (s[n - 1] == '=') pad++;
    if (s[n - 2] == '=') pad++;
    unsigned char *out = malloc(n / 4 * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i += 4) {
        int q[4];
        for (int j = 0; j < 4; j++) {
            unsigned char c = (unsigned char)s[i + j];
            if (c == '=') {
                /* padding only in the final quad, positions 2/3 */
                if (i + 4 != n || j < 2) { free(out); return NULL; }
                q[j] = 0;
            } else {
                q[j] = ka_b64_val(c);
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

/* ── CPython UTF-8 decode(errors="replace") — maximal-subpart rule ──────────
 * One U+FFFD per maximal invalid subsequence: a start byte consumes its valid
 * continuation prefix, then ONE replacement is emitted and decoding resumes
 * at the first invalid byte. Caller frees the UTF-8 result. */
static void ka_sb_cp(ka_sb_t *sb, unsigned long cp) {
    char buf[5];
    if (cp < 0x80) { buf[0] = (char)cp; ka_sb_addn(sb, buf, 1); }
    else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F));
        ka_sb_addn(sb, buf, 2);
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        ka_sb_addn(sb, buf, 3);
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18)); buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[3] = (char)(0x80 | (cp & 0x3F));
        ka_sb_addn(sb, buf, 4);
    }
}
static char *ka_utf8_replace(const unsigned char *s, size_t n) {
    ka_sb_t sb;
    ka_sb_init(&sb);
    size_t i = 0;
    while (i < n) {
        unsigned char b = s[i];
        if (b < 0x80) { ka_sb_cp(&sb, b); i++; continue; }
        if (b < 0xC2) { ka_sb_cp(&sb, 0xFFFD); i++; continue; }
        if (b < 0xE0) {
            if (i + 1 < n && s[i + 1] >= 0x80 && s[i + 1] <= 0xBF) {
                ka_sb_cp(&sb, ((unsigned long)(b & 0x1F) << 6) | (s[i + 1] & 0x3F));
                i += 2;
            } else { ka_sb_cp(&sb, 0xFFFD); i++; }
            continue;
        }
        if (b < 0xF0) {
            unsigned char lo = (b == 0xE0) ? 0xA0 : 0x80;
            unsigned char hi = (b == 0xED) ? 0x9F : 0xBF;
            if (i + 1 < n && s[i + 1] >= lo && s[i + 1] <= hi) {
                if (i + 2 < n && s[i + 2] >= 0x80 && s[i + 2] <= 0xBF) {
                    ka_sb_cp(&sb, ((unsigned long)(b & 0x0F) << 12) |
                                  ((unsigned long)(s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F));
                    i += 3;
                } else { ka_sb_cp(&sb, 0xFFFD); i += 2; }
            } else { ka_sb_cp(&sb, 0xFFFD); i++; }
            continue;
        }
        if (b < 0xF5) {
            unsigned char lo = (b == 0xF0) ? 0x90 : 0x80;
            unsigned char hi = (b == 0xF4) ? 0x8F : 0xBF;
            if (i + 1 < n && s[i + 1] >= lo && s[i + 1] <= hi) {
                if (i + 2 < n && s[i + 2] >= 0x80 && s[i + 2] <= 0xBF) {
                    if (i + 3 < n && s[i + 3] >= 0x80 && s[i + 3] <= 0xBF) {
                        ka_sb_cp(&sb, ((unsigned long)(b & 0x07) << 18) |
                                      ((unsigned long)(s[i + 1] & 0x3F) << 12) |
                                      ((unsigned long)(s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F));
                        i += 4;
                    } else { ka_sb_cp(&sb, 0xFFFD); i += 3; }
                } else { ka_sb_cp(&sb, 0xFFFD); i += 2; }
            } else { ka_sb_cp(&sb, 0xFFFD); i++; }
            continue;
        }
        ka_sb_cp(&sb, 0xFFFD); i++;
    }
    return sb.p ? sb.p : ka_strdup("");
}

/* ── Python int(value) on a JSON node ─────────────────────────────────────── */
static json_t *ka_py_int_node(const json_t *v) {
    if (ka_is_none(v))
        return ka_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        if (isnan(v->num_val))
            return ka_raise("ValueError", "cannot convert float NaN to integer");
        if (isinf(v->num_val))
            return ka_raise("OverflowError", "cannot convert float infinity to integer");
        return json_number(trunc(v->num_val));
    }
    if (v->type == JSON_STRING) {
        const char *raw = v->str_val ? v->str_val : "";
        char *s = ka_int_strip(raw);
        bool ok = s && s[0] != '\0';
        long long acc = 0;
        bool overflow = false;
        if (ok) {
            const char *p = s;
            bool neg = false;
            if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
            const char *digits_start = p;
            bool prev_digit = false, any = false;
            for (; *p; p++) {
                if (*p >= '0' && *p <= '9') {
                    if (acc > (9223372036854775807LL - (*p - '0')) / 10) overflow = true;
                    acc = acc * 10 + (*p - '0');
                    prev_digit = true; any = true;
                } else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
                    prev_digit = false;
                } else { ok = false; break; }
            }
            if (!any || (p > digits_start && p[-1] == '_')) ok = false;
            if (neg) acc = -acc;
        }
        if (ok && !overflow) {
            free(s);
            return json_number((double)acc);
        }
        free(s);
        if (ok && overflow) {
            /* beyond int64: Python ints are arbitrary precision; the fixture
             * domain caps |int| < 2^53 — fail closed rather than wrap. */
            return ka_raise("OverflowError", "int too large for the ported int64 domain");
        }
        char *rep = ka_py_repr_str(raw);
        char *msg = ka_asprintf("invalid literal for int() with base 10: %s", rep);
        free(rep);
        json_t *e = ka_raise("ValueError", msg);
        free(msg);
        return e;
    }
    char *msg = ka_asprintf(
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        ka_type_name(v));
    json_t *e = ka_raise("TypeError", msg);
    free(msg);
    return e;
}

/* ── lexical POSIX paths (PurePosixPath) ──────────────────────────────────── */
/* Parse a path string into root + components (drops '.' / empty components,
 * keeps '..', preserves exactly-two leading slashes). Caller frees root and
 * each component. Returns component count; *root is malloc'd ("" or "/" or
 * "//"). */
typedef struct { char *root; char **parts; size_t n, cap; } ka_path_t;
static void ka_path_init(ka_path_t *p) { p->root = ka_strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }
static void ka_path_push(ka_path_t *p, const char *s, size_t n) {
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
static void ka_path_parse(ka_path_t *p, const char *s) {
    ka_path_init(p);
    const char *q = s ? s : "";
    size_t slashes = 0;
    while (q[slashes] == '/') slashes++;
    if (slashes == 2) { free(p->root); p->root = ka_strdup("//"); }
    else if (slashes >= 1) { free(p->root); p->root = ka_strdup("/"); }
    const char *seg = q;
    for (;;) {
        const char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        ka_path_push(p, seg, len);
        if (!slash) break;
        seg = slash + 1;
    }
}
static void ka_path_free(ka_path_t *p) {
    free(p->root);
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = p->cap = 0;
}
static char *ka_path_str(const ka_path_t *p) {
    ka_sb_t sb;
    ka_sb_init(&sb);
    ka_sb_add(&sb, p->root);
    for (size_t i = 0; i < p->n; i++) {
        if (i) ka_sb_add(&sb, "/");
        ka_sb_add(&sb, p->parts[i]);
    }
    if (sb.len == 0) ka_sb_add(&sb, ".");
    return sb.p ? sb.p : ka_strdup(".");
}
/* PurePosixPath(base).joinpath(*parts): an absolute part resets the path. */
static char *ka_path_join(const char *base, json_t *parts) {
    ka_path_t acc;
    ka_path_parse(&acc, base);
    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parts); i++) {
            json_t *pv = json_get(parts, i);
            const char *s = ka_str(pv);
            if (!s) continue;   /* non-str parts out of fixture domain */
            ka_path_t piece;
            ka_path_parse(&piece, s);
            if (piece.root[0] != '\0') {
                /* absolute component: reset */
                free(acc.root);
                acc.root = ka_strdup(piece.root);
                for (size_t k = 0; k < acc.n; k++) free(acc.parts[k]);
                acc.n = 0;
            }
            for (size_t k = 0; k < piece.n; k++)
                ka_path_push(&acc, piece.parts[k], strlen(piece.parts[k]));
            ka_path_free(&piece);
        }
    }
    char *out = ka_path_str(&acc);
    ka_path_free(&acc);
    return out;
}

/* ── scripted cursor model (mirrors port_kanban_db_flow.c) ────────────────── */
typedef struct { json_t *spec; size_t idx; } ka_cursor_t;
static void ka_cursor_clear(ka_cursor_t *cur) {
    if (cur->spec) json_free(cur->spec);
    cur->spec = NULL;
    cur->idx = 0;
}
/* conn.execute(sql, params) — records verbatim SQL + params; on scripted
 * failure returns the raise sentinel, else fills cur (spec owned by cur). */
static json_t *ka_execute(ka_io_t *io, const char *sql, json_t *params, ka_cursor_t *cur) {
    json_t *args = json_object();
    json_set(args, "sql", json_string(sql));
    json_set(args, "params", params ? params : json_array());
    json_t *r = ka_seam(io, "execute", args);
    cur->spec = NULL;
    cur->idx = 0;
    if (ka_raised(r)) return r;
    cur->spec = r;
    return NULL;
}
static json_t *ka_fetch_raise(const ka_cursor_t *cur) {
    if (!cur->spec || cur->spec->type != JSON_OBJECT) return NULL;
    json_t *fr = json_obj_get(cur->spec, "fetch_raise");
    if (!fr || fr->type != JSON_OBJECT) return NULL;
    return ka_raise_from_spec(fr);
}
static json_t *ka_fetchone(ka_io_t *io, ka_cursor_t *cur) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string("fetchone"));
    json_append(io->trace, ev);
    json_t *fr = ka_fetch_raise(cur);
    if (fr) return fr;
    json_t *rows = (cur->spec && cur->spec->type == JSON_OBJECT) ? json_obj_get(cur->spec, "rows") : NULL;
    if (!rows || rows->type != JSON_ARRAY || cur->idx >= json_len(rows)) return json_null();
    return json_copy(json_get(rows, cur->idx++));
}
static json_t *ka_fetchall(ka_io_t *io, ka_cursor_t *cur) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string("fetchall"));
    json_append(io->trace, ev);
    json_t *fr = ka_fetch_raise(cur);
    if (fr) return fr;
    json_t *out = json_array();
    json_t *rows = (cur->spec && cur->spec->type == JSON_OBJECT) ? json_obj_get(cur->spec, "rows") : NULL;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = cur->idx; i < json_len(rows); i++)
            json_append(out, json_copy(json_get(rows, i)));
        cur->idx = json_len(rows);
    }
    return out;
}
/* Cursor iteration (__iter__ / __next__): one "iter" event, then one "next"
 * event per YIELDED row; the final StopIteration __next__ is untraced. */
static json_t *ka_iter_begin(ka_io_t *io, ka_cursor_t *cur) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string("iter"));
    json_append(io->trace, ev);
    return ka_fetch_raise(cur);
}
static json_t *ka_iter_next(ka_io_t *io, ka_cursor_t *cur, bool *exhausted) {
    *exhausted = false;
    json_t *rows = (cur->spec && cur->spec->type == JSON_OBJECT) ? json_obj_get(cur->spec, "rows") : NULL;
    if (!rows || rows->type != JSON_ARRAY || cur->idx >= json_len(rows)) {
        *exhausted = true;
        return json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "call", json_string("next"));
    json_append(io->trace, ev);
    return json_copy(json_get(rows, cur->idx++));
}
/* Direct row[col] — KeyError when absent (fail closed). */
static json_t *ka_row_col(const json_t *row, const char *col) {
    if (!row || row->type != JSON_OBJECT || !json_has(row, col)) {
        char *msg = ka_asprintf("'%s'", col);
        json_t *e = ka_raise("KeyError", msg);
        free(msg);
        return e;
    }
    return json_copy(json_obj_get(row, col));
}

/* ── write_txn context-manager double ─────────────────────────────────────── */
static json_t *ka_txn_enter(ka_io_t *io) {
    json_t *args = json_object();
    json_set(args, "allow_nested", json_bool(false));
    return ka_seam(io, "write_txn.enter", args);
}
static json_t *ka_txn_exit(ka_io_t *io, const json_t *exc) {
    json_t *args = json_object();
    if (ka_raised(exc)) {
        json_t *e = json_object();
        json_set(e, "type", json_copy(json_obj_get(exc, "__raise__")));
        json_t *m = json_obj_get(exc, "message");
        json_set(e, "message", m ? json_copy(m) : json_string(""));
        json_set(args, "exc", e);
    } else {
        json_set(args, "exc", json_null());
    }
    return ka_seam(io, "write_txn.exit", args);
}

/* ── composed ported siblings (real on both sides) ────────────────────────── */
static json_t *ka_first_line_node(const json_t *text, long long limit);
static json_t *ka_explicit_board_slug(ka_io_t *io, const json_t *board);
static json_t *ka_explicit_board_intent_pinned(ka_io_t *io);

/* ═══════════════════════════════════════════════════════════════════════════
 * PoP-ported functions (all UNCLAIMED hermes_cli/kanban_db.py defs)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* PoP: ka_lossy_text @ hermes_cli/kanban_db.py:_lossy_text */
json_t *ka_lossy_text(ka_io_t *io, const json_t *value) {
    (void)io;
    /* bytes double: {"__bytes__": "<base64>"} -> utf-8 decode(errors=replace) */
    if (value && value->type == JSON_OBJECT && json_len(value) == 1 &&
        json_has(value, "__bytes__")) {
        const char *b64 = json_get_str(value, "__bytes__", "");
        size_t n = 0;
        unsigned char *raw = ka_b64_decode(b64, &n);
        if (!raw)
            return ka_raise("ValueError", "invalid canonical base64 bytes double");
        char *dec = ka_utf8_replace(raw, n);
        free(raw);
        json_t *r = json_string(dec ? dec : "");
        free(dec);
        return r;
    }
    return value ? json_copy(value) : json_null();
}

/* PoP: ka_row_get @ hermes_cli/kanban_db.py:_row_get */
json_t *ka_row_get(ka_io_t *io, const json_t *row, const char *col, const json_t *deflt) {
    (void)io;
    if (ka_is_none(row)) return deflt ? json_copy(deflt) : json_null();
    if (row->type != JSON_OBJECT) return ka_attr_error(row, "keys");
    if (!json_has(row, col)) return deflt ? json_copy(deflt) : json_null();
    return json_copy(json_obj_get(row, col));
}

/* PoP: ka_json_or @ hermes_cli/kanban_db.py:_json_or */
json_t *ka_json_or(ka_io_t *io, const json_t *value, const json_t *deflt) {
    (void)io;
    json_t *d = deflt ? json_copy(deflt) : json_null();
    if (!ka_truthy(value)) return d;
    if (value->type != JSON_STRING) {
        /* json.loads(non-str) raises TypeError -> except Exception -> default */
        return d;
    }
    const char *s = value->str_val ? value->str_val : "";
    if (!ka_json_strict_valid(s)) return d;
    char *err = NULL;
    json_t *parsed = json_parse(s, &err);
    free(err);
    if (!parsed) return d;
    json_free(d);
    return parsed;
}

/* PoP: ka_json_dict @ hermes_cli/kanban_db.py:_json_dict */
json_t *ka_json_dict(ka_io_t *io, const json_t *value) {
    json_t *empty = json_object();
    json_t *parsed = ka_json_or(io, value, empty);
    json_free(empty);
    if (ka_raised(parsed)) return parsed;
    if (parsed && parsed->type == JSON_OBJECT) return parsed;
    json_free(parsed);
    return json_object();
}

/* PoP: ka_env_int @ hermes_cli/kanban_db.py:_env_int */
json_t *ka_env_int(ka_io_t *io, const char *name, const json_t *deflt, long long minimum) {
    json_t *args = json_object();
    json_set(args, "name", json_string(name ? name : ""));
    json_set(args, "default", json_string(""));
    json_t *raw = ka_seam(io, "environ.get", args);
    if (ka_raised(raw)) return raw;
    if (!raw || raw->type != JSON_STRING) {
        json_t *e = ka_attr_error(raw, "strip");
        json_free(raw);
        return e;
    }
    char *stripped = ka_strip(raw->str_val ? raw->str_val : "");
    json_free(raw);
    json_t *out = deflt ? json_copy(deflt) : json_null();
    if (stripped && stripped[0] != '\0') {
        json_t *sv = json_string(stripped);
        json_t *parsed = ka_py_int_node(sv);
        json_free(sv);
        if (ka_raised(parsed)) {
            /* except ValueError only; other kinds propagate */
            if (ka_exc_isinstance(json_get_str(parsed, "__raise__", ""), "ValueError")) {
                json_free(parsed);
                free(stripped);
                return out;
            }
            json_free(out);
            free(stripped);
            return parsed;
        }
        if (parsed->num_val >= (double)minimum) {
            json_free(out);
            out = parsed;
        } else {
            json_free(parsed);
        }
    }
    free(stripped);
    return out;
}

/* PoP: ka_first_line @ hermes_cli/kanban_db.py:_first_line */
json_t *ka_first_line(ka_io_t *io, const json_t *text, long long limit) {
    (void)io;
    return ka_first_line_node(text, limit);
}
/* lines[0][:limit] with Python slice semantics (negative limit drops from the
 * end; 0 -> ""). Caller-owned json string. */
static json_t *ka_first_line_node(const json_t *text, long long limit) {
    /* (text or "") — a truthy non-str hits .strip() AttributeError; falsy
     * non-str (0/false/[]/{}) projects to "". Fixture domain: str|null. */
    const char *raw;
    if (ka_is_none(text) || !ka_truthy(text)) raw = "";
    else if (text->type == JSON_STRING) raw = text->str_val ? text->str_val : "";
    else return ka_attr_error(text, "strip");
    char *stripped = ka_strip(raw);
    if (!stripped) return json_string("");
    const unsigned char *p = (const unsigned char *)stripped;
    const unsigned char *end = p + strlen(stripped);
    const unsigned char *line_end = p;
    while (line_end < end && !ka_line_sep(line_end)) line_end++;
    /* count code points in the first line */
    size_t line_cps = 0;
    {
        const unsigned char *q = p;
        while (q < line_end) { ka_next_cp(&q); line_cps++; }
    }
    size_t take;
    if (limit < 0) {
        long long t = (long long)line_cps + limit;
        take = t > 0 ? (size_t)t : 0;
    } else {
        take = (size_t)limit < line_cps ? (size_t)limit : line_cps;
    }
    const unsigned char *q = p;
    for (size_t i = 0; i < take; i++) ka_next_cp(&q);
    size_t n = (size_t)(q - p);
    char *out = malloc(n + 1);
    if (!out) { free(stripped); return json_string(""); }
    memcpy(out, p, n);
    out[n] = '\0';
    free(stripped);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: ka_opt_int @ hermes_cli/kanban_db.py:_opt_int */
json_t *ka_opt_int(ka_io_t *io, const json_t *value) {
    (void)io;
    if (ka_is_none(value)) return json_null();
    return ka_py_int_node(value);
}

/* PoP: ka_json_or_null @ hermes_cli/kanban_db.py:_json_or_null */
json_t *ka_json_or_null(ka_io_t *io, const json_t *obj) {
    (void)io;
    if (!ka_truthy(obj)) return json_null();
    char *s = ka_py_dumps(obj);
    json_t *r = json_string(s ? s : "null");
    free(s);
    return r;
}

/* PoP: ka_substantive_text @ hermes_cli/kanban_db.py:_substantive_text */
json_t *ka_substantive_text(ka_io_t *io, const json_t *value) {
    (void)io;
    bool r = false;
    if (!ka_is_none(value)) {
        if (value->type == JSON_STRING) {
            char *s = ka_strip(value->str_val ? value->str_val : "");
            r = s && s[0] != '\0';
            free(s);
        } else {
            /* str() of any other JSON value is non-blank (numbers, bools,
             * '[]'/'{}' containers). */
            r = true;
        }
    }
    return json_bool(r);
}

/* PoP: ka_nonblank_str @ hermes_cli/kanban_db.py:_nonblank_str */
json_t *ka_nonblank_str(ka_io_t *io, const json_t *value) {
    (void)io;
    if (value && value->type == JSON_STRING) {
        char *s = ka_strip(value->str_val ? value->str_val : "");
        bool nonblank = s && s[0] != '\0';
        free(s);
        if (nonblank) return json_copy(value);
    }
    return json_null();
}

/* PoP: ka_cleaned_artifact_paths @ hermes_cli/kanban_db.py:_cleaned_artifact_paths */
json_t *ka_cleaned_artifact_paths(ka_io_t *io, const json_t *metadata) {
    (void)io;
    json_t *out = json_array();
    if (!metadata || metadata->type != JSON_OBJECT) return out;
    json_t *raw = json_obj_get(metadata, "artifacts");
    if (!raw || raw->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(raw); i++) {
        json_t *p = json_get(raw, i);
        if (!p || p->type != JSON_STRING) continue;
        char *s = ka_strip(p->str_val ? p->str_val : "");
        if (s && s[0] != '\0') json_append(out, json_string(s));
        free(s);
    }
    return out;
}

/* PoP: ka_retention_seconds @ hermes_cli/kanban_db.py:_retention_seconds */
json_t *ka_retention_seconds(ka_io_t *io, const json_t *older_than_seconds) {
    (void)io;
    json_t *v = ka_py_int_node(older_than_seconds);
    if (ka_raised(v)) return v;
    if (v->num_val < 0) {
        char *msg = ka_asprintf(
            "older_than_seconds must be >= 0, got %lld: "
            "a negative retention selects everything.", (long long)v->num_val);
        json_free(v);
        json_t *e = ka_raise("ValueError", msg);
        free(msg);
        return e;
    }
    return v;
}

/* PoP: ka_slug_or_default @ hermes_cli/kanban_db.py:_slug_or_default */
json_t *ka_slug_or_default(ka_io_t *io, const json_t *board) {
    json_t *args = json_object();
    json_set(args, "slug", board ? json_copy(board) : json_null());
    json_t *r = ka_seam(io, "_normalize_board_slug", args);
    if (ka_raised(r)) return r;
    if (ka_truthy(r)) return r;
    json_free(r);
    return json_string(KA_DEFAULT_BOARD);
}

/* PoP: ka_require_slug @ hermes_cli/kanban_db.py:_require_slug */
json_t *ka_require_slug(ka_io_t *io, const json_t *slug) {
    json_t *args = json_object();
    json_set(args, "slug", slug ? json_copy(slug) : json_null());
    json_t *r = ka_seam(io, "_normalize_board_slug", args);
    if (ka_raised(r)) return r;
    if (!ka_truthy(r)) {
        json_free(r);
        return ka_raise("ValueError", "board slug is required");
    }
    return r;
}

/* PoP: ka_host_prefix @ hermes_cli/kanban_db.py:_host_prefix */
json_t *ka_host_prefix(ka_io_t *io) {
    json_t *r = ka_seam(io, "_claimer_id", json_object());
    if (ka_raised(r)) return r;
    if (!r || r->type != JSON_STRING) {
        json_t *e = ka_attr_error(r, "split");
        json_free(r);
        return e;
    }
    const char *s = r->str_val ? r->str_val : "";
    const char *colon = strchr(s, ':');
    size_t n = colon ? (size_t)(colon - s) : strlen(s);
    char *out = malloc(n + 2);
    if (!out) { json_free(r); return json_string(":"); }
    memcpy(out, s, n);
    out[n] = ':';
    out[n + 1] = '\0';
    json_free(r);
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* PoP: ka_board_meta_for @ hermes_cli/kanban_db.py:_board_meta_for */
json_t *ka_board_meta_for(ka_io_t *io, const json_t *board) {
    json_t *resolved;
    if (ka_truthy(board)) {
        resolved = json_copy(board);
    } else {
        resolved = ka_seam(io, "get_current_board", json_object());
        if (ka_raised(resolved)) return resolved;
    }
    json_t *args = json_object();
    json_set(args, "board", resolved);
    return ka_seam(io, "read_board_metadata", args);
}

/* PoP: ka_hook_profile_name @ hermes_cli/kanban_db.py:_hook_profile_name */
json_t *ka_hook_profile_name(ka_io_t *io) {
    /* the live import sits OUTSIDE the try: an import failure propagates
     * untouched (only the CALL is guarded by except Exception). */
    json_t *imp = ka_seam(io, "import:hermes_cli.profiles", json_object());
    if (ka_raised(imp)) return imp;
    json_free(imp);
    json_t *r = ka_seam(io, "get_active_profile_name", json_object());
    if (ka_raised(r)) {
        if (ka_exc_isinstance(json_get_str(r, "__raise__", ""), "Exception")) {
            json_free(r);
            return json_string("default");
        }
        return r;
    }
    return r;
}

/* PoP: ka_fire_task_hook @ hermes_cli/kanban_db.py:_fire_task_hook */
json_t *ka_fire_task_hook(ka_io_t *io, const char *event, const json_t *task,
                          const char *task_id, const json_t *run_id, const json_t *fields) {
    /* kwarg evaluation order: board=get_current_board(), then
     * assignee=task.assignee if task else None, then run_id, then **fields.
     * The task double is a SimpleNamespace (ALWAYS truthy when present, even
     * empty) — model presence, not jtruth ({} is falsy in C, truthy live). */
    json_t *board = ka_seam(io, "get_current_board", json_object());
    if (ka_raised(board)) return board;
    json_t *assignee = json_null();
    if (!ka_is_none(task)) {
        if (!task || task->type != JSON_OBJECT || !json_has(task, "assignee")) {
            json_free(board);
            json_t *e = ka_raise("AttributeError",
                "'types.SimpleNamespace' object has no attribute 'assignee'");
            return e;
        }
        json_free(assignee);
        assignee = json_copy(json_obj_get(task, "assignee"));
    }
    json_t *args = json_object();
    json_set(args, "event", json_string(event ? event : ""));
    json_set(args, "task_id", json_string(task_id ? task_id : ""));
    json_set(args, "board", board);
    json_set(args, "assignee", assignee);
    json_set(args, "run_id", run_id ? json_copy(run_id) : json_null());
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = fields->c.keys[i];
            if (strcmp(k, "event") == 0 || strcmp(k, "task_id") == 0 ||
                strcmp(k, "board") == 0 || strcmp(k, "assignee") == 0 ||
                strcmp(k, "run_id") == 0) {
                /* out of fixture domain: binding collision (fail closed) */
                char *msg = ka_asprintf(
                    "_fire_kanban_lifecycle_hook() got multiple values for argument '%s'", k);
                json_t *e = ka_raise("TypeError", msg);
                free(msg);
                json_free(args);
                return e;
            }
            json_set(args, k, json_copy(fields->c.items[i]));
        }
    }
    json_t *r = ka_seam(io, "_fire_kanban_lifecycle_hook", args);
    if (ka_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ka_explicit_board_slug @ hermes_cli/kanban_db.py:_explicit_board_slug */
json_t *ka_explicit_board_slug(ka_io_t *io, const json_t *board) {
    if (!ka_is_none(board)) {
        json_t *args = json_object();
        json_set(args, "slug", json_copy(board));
        return ka_seam(io, "_normalize_board_slug", args);
    }
    json_t *ctx = ka_seam(io, "current_board_override.get", json_object());
    if (ka_raised(ctx)) return ctx;
    /* (ctx or "").strip() */
    const char *s = "";
    if (ka_truthy(ctx)) {
        if (ctx->type != JSON_STRING) {
            json_t *e = ka_attr_error(ctx, "strip");
            json_free(ctx);
            return e;
        }
        s = ctx->str_val ? ctx->str_val : "";
    }
    char *stripped = ka_strip(s);
    json_free(ctx);
    if (!stripped || stripped[0] == '\0') {
        free(stripped);
        return json_null();
    }
    json_t *args = json_object();
    json_set(args, "slug", json_string(stripped));
    free(stripped);
    json_t *r = ka_seam(io, "_normalize_board_slug", args);
    if (ka_raised(r)) {
        if (ka_exc_isinstance(json_get_str(r, "__raise__", ""), "ValueError")) {
            json_free(r);
            return json_null();
        }
        return r;
    }
    return r;
}

/* PoP: ka_explicit_board_intent_pinned @ hermes_cli/kanban_db.py:_explicit_board_intent_pinned */
json_t *ka_explicit_board_intent_pinned(ka_io_t *io) {
    json_t *pin = ka_seam(io, "pin_first.get", json_object());
    if (ka_raised(pin)) return pin;
    bool truthy = ka_truthy(pin);
    json_free(pin);
    if (truthy) return json_bool(true);
    json_t *imp = ka_seam(io, "import:agent.delegation_context", json_object());
    if (ka_raised(imp)) return imp;   /* no handler: propagates */
    json_free(imp);
    return ka_seam(io, "explicit_board_intent_is_pinned", json_object());
}

/* PoP: ka_board_path @ hermes_cli/kanban_db.py:_board_path */
json_t *ka_board_path(ka_io_t *io, const json_t *env_var, const json_t *board,
                      json_t *default_parts, const char *leaf) {
    /* pin = os.environ.get(env_var, "").strip() if env_var else "" */
    char *pin = ka_strdup("");
    if (ka_truthy(env_var)) {
        if (env_var->type != JSON_STRING) {
            free(pin);
            return ka_attr_error(env_var, "strip");
        }
        json_t *args = json_object();
        json_set(args, "name", json_copy(env_var));
        json_set(args, "default", json_string(""));
        json_t *raw = ka_seam(io, "environ.get", args);
        if (ka_raised(raw)) { free(pin); return raw; }
        if (!raw || raw->type != JSON_STRING) {
            json_t *e = ka_attr_error(raw, "strip");
            json_free(raw);
            free(pin);
            return e;
        }
        free(pin);
        pin = ka_strip(raw->str_val ? raw->str_val : "");
        json_free(raw);
        if (!pin) pin = ka_strdup("");
    }
    /* slug = _explicit_board_slug(board) — composed ported sibling */
    json_t *slug = ka_explicit_board_slug(io, board);
    if (ka_raised(slug)) { free(pin); return slug; }
    /* if pin and (slug is None or _explicit_board_intent_pinned()) */
    if (pin[0] != '\0') {
        bool use_pin = ka_is_none(slug);
        if (!use_pin) {
            json_t *pinned = ka_explicit_board_intent_pinned(io);
            if (ka_raised(pinned)) { free(pin); json_free(slug); return pinned; }
            use_pin = ka_truthy(pinned);
            json_free(pinned);
        }
        if (use_pin) {
            /* Path(pin).expanduser() — parse first, then the seam */
            ka_path_t parsed;
            ka_path_parse(&parsed, pin);
            char *pstr = ka_path_str(&parsed);
            ka_path_free(&parsed);
            json_t *args = json_object();
            json_set(args, "path", json_string(pstr));
            free(pstr);
            json_t *exp = ka_seam(io, "expanduser", args);
            free(pin);
            json_free(slug);
            if (ka_raised(exp)) return exp;
            const char *es = ka_str(exp);
            ka_path_t outp;
            ka_path_parse(&outp, es ? es : "");
            char *outs = ka_path_str(&outp);
            ka_path_free(&outp);
            json_free(exp);
            json_t *r = json_string(outs);
            free(outs);
            return r;
        }
    }
    free(pin);
    if (ka_is_none(slug)) {
        json_free(slug);
        slug = ka_seam(io, "get_current_board", json_object());
        if (ka_raised(slug)) return slug;
    }
    const char *sl = ka_str(slug);
    if (sl && strcmp(sl, KA_DEFAULT_BOARD) == 0) {
        json_free(slug);
        json_t *home = ka_seam(io, "kanban_home", json_object());
        if (ka_raised(home)) return home;
        const char *hs = ka_str(home);
        char *joined = ka_path_join(hs ? hs : "", default_parts);
        json_free(home);
        json_t *r = json_string(joined);
        free(joined);
        return r;
    }
    json_t *args = json_object();
    json_set(args, "slug", slug);   /* consumes slug */
    json_t *dir = ka_seam(io, "board_dir", args);
    if (ka_raised(dir)) return dir;
    const char *ds = ka_str(dir);
    json_t *leaf_arr = json_array();
    json_append(leaf_arr, json_string(leaf ? leaf : ""));
    char *joined = ka_path_join(ds ? ds : "", leaf_arr);
    json_free(leaf_arr);
    json_free(dir);
    json_t *r = json_string(joined);
    free(joined);
    return r;
}

/* PoP: ka_link @ hermes_cli/kanban_db.py:_link */
json_t *ka_link(ka_io_t *io, const char *parent_id, const char *child_id) {
    json_t *params = json_array();
    json_append(params, json_string(parent_id ? parent_id : ""));
    json_append(params, json_string(child_id ? child_id : ""));
    ka_cursor_t cur;
    json_t *err = ka_execute(io, KA_SQL_LINK, params, &cur);
    if (err) return err;
    ka_cursor_clear(&cur);
    return json_null();
}

/* PoP: ka_linked_ids @ hermes_cli/kanban_db.py:_linked_ids */
json_t *ka_linked_ids(ka_io_t *io, const char *want, const char *where, const char *task_id) {
    ka_sb_t sql;
    ka_sb_init(&sql);
    ka_sb_add(&sql, KA_SQL_LINKED_PRE);
    ka_sb_add(&sql, want ? want : "");
    ka_sb_add(&sql, KA_SQL_LINKED_MID);
    ka_sb_add(&sql, where ? where : "");
    ka_sb_add(&sql, KA_SQL_LINKED_POST);
    ka_sb_add(&sql, want ? want : "");
    json_t *params = json_array();
    json_append(params, json_string(task_id ? task_id : ""));
    ka_cursor_t cur;
    json_t *err = ka_execute(io, sql.p ? sql.p : "", params, &cur);
    ka_sb_free(&sql);
    if (err) return err;
    json_t *rows = ka_fetchall(io, &cur);
    ka_cursor_clear(&cur);
    if (ka_raised(rows)) return rows;
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *r = json_get(rows, i);
        json_t *v = ka_row_col(r, want ? want : "");
        if (ka_raised(v)) { json_free(rows); json_free(out); return v; }
        json_append(out, v);
    }
    json_free(rows);
    return out;
}

/* PoP: ka_require_task @ hermes_cli/kanban_db.py:_require_task */
json_t *ka_require_task(ka_io_t *io, const char *task_id) {
    json_t *params = json_array();
    json_append(params, json_string(task_id ? task_id : ""));
    ka_cursor_t cur;
    json_t *err = ka_execute(io, KA_SQL_REQUIRE_TASK, params, &cur);
    if (err) return err;
    json_t *row = ka_fetchone(io, &cur);
    ka_cursor_clear(&cur);
    if (ka_raised(row)) return row;
    if (!ka_truthy(row)) {
        json_free(row);
        char *msg = ka_asprintf("unknown task %s", task_id ? task_id : "");
        json_t *e = ka_raise("ValueError", msg);
        free(msg);
        return e;
    }
    json_free(row);
    return json_null();
}

/* PoP: ka_task_rows @ hermes_cli/kanban_db.py:_task_rows */
json_t *ka_task_rows(ka_io_t *io, const char *table, const char *task_id, const char *order) {
    ka_sb_t sql;
    ka_sb_init(&sql);
    ka_sb_add(&sql, KA_SQL_TASK_ROWS_PRE);
    ka_sb_add(&sql, table ? table : "");
    ka_sb_add(&sql, KA_SQL_TASK_ROWS_MID);
    ka_sb_add(&sql, order ? order : "");
    json_t *params = json_array();
    json_append(params, json_string(task_id ? task_id : ""));
    ka_cursor_t cur;
    json_t *err = ka_execute(io, sql.p ? sql.p : "", params, &cur);
    ka_sb_free(&sql);
    if (err) return err;
    json_t *rows = ka_fetchall(io, &cur);
    ka_cursor_clear(&cur);
    return rows;
}

/* PoP: ka_latest_event @ hermes_cli/kanban_db.py:_latest_event */
json_t *ka_latest_event(ka_io_t *io, const char *task_id, const char *kind, const json_t *run_id) {
    ka_sb_t sql;
    ka_sb_init(&sql);
    ka_sb_add(&sql, KA_SQL_LATEST_BASE);
    json_t *params = json_array();
    json_append(params, json_string(task_id ? task_id : ""));
    json_append(params, json_string(kind ? kind : ""));
    if (!ka_is_none(run_id)) {
        ka_sb_add(&sql, KA_SQL_LATEST_RUN);
        json_t *rid = ka_py_int_node(run_id);
        if (ka_raised(rid)) {
            ka_sb_free(&sql);
            json_free(params);
            return rid;
        }
        json_append(params, rid);
    }
    ka_sb_add(&sql, KA_SQL_LATEST_ORDER);
    ka_cursor_t cur;
    json_t *err = ka_execute(io, sql.p ? sql.p : "", params, &cur);
    ka_sb_free(&sql);
    if (err) return err;
    json_t *row = ka_fetchone(io, &cur);
    ka_cursor_clear(&cur);
    return row;
}

/* PoP: ka_counts_by_assignee @ hermes_cli/kanban_db.py:_counts_by_assignee */
json_t *ka_counts_by_assignee(ka_io_t *io) {
    ka_cursor_t cur;
    json_t *err = ka_execute(io, KA_SQL_COUNTS, NULL, &cur);
    if (err) return err;
    json_t *ir = ka_iter_begin(io, &cur);
    if (ir) { ka_cursor_clear(&cur); return ir; }
    json_t *counts = json_object();
    for (;;) {
        bool exhausted = false;
        json_t *row = ka_iter_next(io, &cur, &exhausted);
        if (exhausted) break;
        json_t *a = ka_row_col(row, "assignee");
        if (ka_raised(a)) { json_free(row); json_free(counts); ka_cursor_clear(&cur); return a; }
        json_t *st = ka_row_col(row, "status");
        if (ka_raised(st)) { json_free(row); json_free(a); json_free(counts); ka_cursor_clear(&cur); return st; }
        json_t *n = ka_row_col(row, "n");
        if (ka_raised(n)) { json_free(row); json_free(a); json_free(st); json_free(counts); ka_cursor_clear(&cur); return n; }
        json_t *ni = ka_py_int_node(n);
        json_free(n);
        if (ka_raised(ni)) { json_free(row); json_free(a); json_free(st); json_free(counts); ka_cursor_clear(&cur); return ni; }
        const char *ak = ka_str(a);
        const char *sk = ka_str(st);
        if (!ak || !sk) {
            /* hashable non-str keys out of fixture domain (fail closed) */
            json_free(row); json_free(a); json_free(st); json_free(ni); json_free(counts);
            ka_cursor_clear(&cur);
            return ka_raise("TypeError", "unhashable assignee/status key (out of domain)");
        }
        json_t *sub = json_obj_get(counts, ak);
        if (!sub) {
            sub = json_object();
            json_set(counts, ak, sub);
        }
        json_set(sub, sk, ni);   /* consumes ni */
        json_free(row); json_free(a); json_free(st);
    }
    ka_cursor_clear(&cur);
    return counts;
}

/* PoP: ka_current_run_started_ats @ hermes_cli/kanban_db.py:current_run_started_ats */
json_t *ka_current_run_started_ats(ka_io_t *io, json_t *task_ids) {
    json_t *ids = json_array();
    if (task_ids && task_ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(task_ids); i++)
            json_append(ids, json_copy(json_get(task_ids, i)));
    }
    if (json_len(ids) == 0) { json_free(ids); return json_object(); }
    ka_sb_t sql;
    ka_sb_init(&sql);
    ka_sb_add(&sql, KA_SQL_RUN_STARTED_PRE);
    for (size_t i = 0; i < json_len(ids); i++) {
        if (i) ka_sb_add(&sql, ",");
        ka_sb_add(&sql, "?");
    }
    ka_sb_add(&sql, KA_SQL_RUN_STARTED_POST);
    ka_cursor_t cur;
    json_t *err = ka_execute(io, sql.p ? sql.p : "", ids, &cur);
    ka_sb_free(&sql);
    if (err) return err;
    json_t *rows = ka_fetchall(io, &cur);
    ka_cursor_clear(&cur);
    if (ka_raised(rows)) return rows;
    json_t *out = json_object();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *r = json_get(rows, i);
        json_t *tid = ka_row_col(r, "task_id");
        if (ka_raised(tid)) { json_free(rows); json_free(out); return tid; }
        json_t *sa = ka_row_col(r, "started_at");
        if (ka_raised(sa)) { json_free(rows); json_free(tid); json_free(out); return sa; }
        const char *k = ka_str(tid);
        if (!k) {
            json_free(rows); json_free(tid); json_free(sa); json_free(out);
            return ka_raise("TypeError", "unhashable task_id key (out of domain)");
        }
        json_set(out, k, sa);   /* consumes sa; last wins on duplicates */
        json_free(tid);
    }
    json_free(rows);
    return out;
}

/* PoP: ka_end_or_synthesize_run @ hermes_cli/kanban_db.py:_end_or_synthesize_run */
json_t *ka_end_or_synthesize_run(ka_io_t *io, const char *task_id, const char *outcome,
                                 const char *status, const json_t *summary,
                                 const json_t *metadata, bool synthesize,
                                 const json_t *profile, bool profile_given) {
    json_t *args = json_object();
    json_set(args, "task_id", json_string(task_id ? task_id : ""));
    json_set(args, "outcome", json_string(outcome ? outcome : ""));
    json_set(args, "status", json_string(status ? status : ""));
    json_set(args, "summary", summary ? json_copy(summary) : json_null());
    json_set(args, "error", json_null());
    json_set(args, "metadata", metadata ? json_copy(metadata) : json_null());
    json_t *run_id = ka_seam(io, "_end_run", args);
    if (ka_raised(run_id)) return run_id;
    /* `run_id is None` — identity, not truthiness (0 is a valid run id). */
    if (ka_is_none(run_id) && synthesize) {
        json_free(run_id);
        json_t *sargs = json_object();
        json_set(sargs, "task_id", json_string(task_id ? task_id : ""));
        json_set(sargs, "outcome", json_string(outcome ? outcome : ""));
        json_set(sargs, "summary", summary ? json_copy(summary) : json_null());
        json_set(sargs, "error", json_null());
        json_set(sargs, "metadata", metadata ? json_copy(metadata) : json_null());
        /* profile=_UNSET projects to null in the seam args (kdbf convention) */
        json_set(sargs, "profile",
                 (profile_given && profile) ? json_copy(profile) : json_null());
        return ka_seam(io, "_synthesize_ended_run", sargs);
    }
    return run_id;
}

/* PoP: ka_gate_created_cards @ hermes_cli/kanban_db.py:_gate_created_cards */
json_t *ka_gate_created_cards(ka_io_t *io, const char *task_id, const json_t *created_cards,
                              const json_t *preview_text) {
    if (!ka_truthy(created_cards)) return json_array();
    json_t *args = json_object();
    json_set(args, "task_id", json_string(task_id ? task_id : ""));
    json_set(args, "created_cards", json_copy(created_cards));
    json_t *pair = ka_seam(io, "_verify_created_cards", args);
    if (ka_raised(pair)) return pair;
    /* tuple unpack: (verified_cards, phantom_cards) */
    if (ka_is_none(pair)) {
        json_free(pair);
        return ka_raise("TypeError", "cannot unpack non-iterable NoneType object");
    }
    if (pair->type != JSON_ARRAY) {
        char *msg = ka_asprintf("cannot unpack non-iterable %s object", ka_type_name(pair));
        json_free(pair);
        json_t *e = ka_raise("TypeError", msg);
        free(msg);
        return e;
    }
    if (json_len(pair) != 2) {
        char *msg = ka_asprintf("%s values to unpack (expected 2, got %zu)",
                                json_len(pair) < 2 ? "not enough" : "too many",
                                json_len(pair));
        json_free(pair);
        json_t *e = ka_raise("ValueError", msg);
        free(msg);
        return e;
    }
    json_t *verified = json_copy(json_get(pair, 0));
    json_t *phantom = json_copy(json_get(pair, 1));
    json_free(pair);
    if (ka_truthy(phantom)) {
        /* with write_txn(conn): _append_event(...) */
        json_t *tx = ka_txn_enter(io);
        if (ka_raised(tx)) { json_free(verified); json_free(phantom); return tx; }
        json_free(tx);
        json_t *payload = json_object();
        json_set(payload, "phantom_cards", json_copy(phantom));
        json_set(payload, "verified_cards", json_copy(verified));
        json_t *preview = ka_first_line_node(preview_text, 200);
        if (preview->type == JSON_STRING && preview->str_val && preview->str_val[0] == '\0') {
            json_free(preview);
            preview = json_null();
        }
        json_set(payload, "summary_preview", preview);
        json_t *eargs = json_object();
        json_set(eargs, "task_id", json_string(task_id ? task_id : ""));
        json_set(eargs, "kind", json_string("completion_blocked_hallucination"));
        json_set(eargs, "payload", payload);
        json_set(eargs, "run_id", json_null());
        json_t *ev = ka_seam(io, "_append_event", eargs);
        json_t *ex = ka_txn_exit(io, ev);
        if (ka_raised(ex)) {
            json_free(ev); json_free(verified); json_free(phantom);
            return ex;
        }
        json_free(ex);
        if (ka_raised(ev)) { json_free(verified); json_free(phantom); return ev; }
        json_free(ev);
        /* raise HallucinatedCardsError(phantom_cards, task_id) */
        ka_sb_t sb;
        ka_sb_init(&sb);
        ka_sb_add(&sb, "completion blocked: claimed created_cards that do not exist "
                       "or were not created by this worker: ");
        if (phantom->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(phantom); i++) {
                if (i) ka_sb_add(&sb, ", ");
                json_t *p = json_get(phantom, i);
                const char *ps = ka_str(p);
                if (ps) ka_sb_add(&sb, ps);
                else {
                    /* str() of a non-str element (out of domain: fail closed) */
                    char *d = ka_py_dumps(p);
                    ka_sb_add(&sb, d);
                    free(d);
                }
            }
        }
        json_t *e = ka_raise("HallucinatedCardsError", sb.p ? sb.p : "");
        ka_sb_free(&sb);
        json_free(verified);
        json_free(phantom);
        return e;
    }
    json_free(phantom);
    return verified;
}

/* PoP: ka_flag_phantom_prose_refs @ hermes_cli/kanban_db.py:_flag_phantom_prose_refs */
json_t *ka_flag_phantom_prose_refs(ka_io_t *io, const char *task_id, const json_t *run_id,
                                   const json_t *summary, const json_t *result,
                                   const json_t *verified_cards) {
    /* scan_text = " ".join(filter(None, [summary, result])) */
    ka_sb_t sb;
    ka_sb_init(&sb);
    const json_t *parts[2] = { summary, result };
    bool first = true;
    for (int i = 0; i < 2; i++) {
        if (!ka_truthy(parts[i])) continue;
        const char *s = ka_str(parts[i]);
        if (!s) {
            /* non-str truthy element: str() in join (out of domain) */
            ka_sb_free(&sb);
            return ka_raise("TypeError", "sequence item: expected str instance (out of domain)");
        }
        if (!first) ka_sb_add(&sb, " ");
        ka_sb_add(&sb, s);
        first = false;
    }
    if (sb.len == 0) { ka_sb_free(&sb); return json_null(); }
    json_t *args = json_object();
    json_set(args, "text", json_string(sb.p));
    ka_sb_free(&sb);
    json_t *scanned = ka_seam(io, "_scan_prose_for_phantom_ids", args);
    if (ka_raised(scanned)) return scanned;
    /* [p for p in scanned if p not in set(verified_cards)] */
    json_t *phantom_refs = json_array();
    if (scanned && scanned->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(scanned); i++) {
            json_t *p = json_get(scanned, i);
            const char *ps = ka_str(p);
            bool in_verified = false;
            if (ps && verified_cards && verified_cards->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_len(verified_cards); j++) {
                    const char *vs = ka_str(json_get(verified_cards, j));
                    if (vs && strcmp(vs, ps) == 0) { in_verified = true; break; }
                }
            }
            if (!in_verified) json_append(phantom_refs, json_copy(p));
        }
    }
    json_free(scanned);
    if (ka_truthy(phantom_refs)) {
        json_t *tx = ka_txn_enter(io);
        if (ka_raised(tx)) { json_free(phantom_refs); return tx; }
        json_free(tx);
        json_t *payload = json_object();
        json_set(payload, "phantom_refs", phantom_refs);   /* consumes */
        json_set(payload, "source", json_string("completion_summary"));
        json_t *eargs = json_object();
        json_set(eargs, "task_id", json_string(task_id ? task_id : ""));
        json_set(eargs, "kind", json_string("suspected_hallucinated_references"));
        json_set(eargs, "payload", payload);
        json_set(eargs, "run_id", run_id ? json_copy(run_id) : json_null());
        json_t *ev = ka_seam(io, "_append_event", eargs);
        json_t *ex = ka_txn_exit(io, ev);
        if (ka_raised(ex)) { json_free(ev); return ex; }
        json_free(ex);
        if (ka_raised(ev)) return ev;
        json_free(ev);
    } else {
        json_free(phantom_refs);
    }
    return json_null();
}

/* PoP: ka_git_out @ hermes_cli/kanban_db.py:_git_out */
json_t *ka_git_out(ka_io_t *io, const char *cwd, json_t *args_list, long long timeout) {
    /* subprocess.run(["git", "-C", str(cwd), *args], capture_output=True,
     *                text=True, encoding="utf-8", errors="replace",
     *                timeout=timeout, check=False) */
    json_t *argv = json_array();
    json_append(argv, json_string("git"));
    json_append(argv, json_string("-C"));
    json_append(argv, json_string(cwd ? cwd : ""));
    if (args_list && args_list->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(args_list); i++) {
            json_t *a = json_get(args_list, i);
            const char *as = ka_str(a);
            if (!as) {
                json_free(argv);
                return ka_raise("TypeError", "git args must be strings (out of domain)");
            }
            json_append(argv, json_string(as));
        }
    }
    json_t *rargs = json_object();
    json_set(rargs, "args", argv);
    json_set(rargs, "capture_output", json_bool(true));
    json_set(rargs, "text", json_bool(true));
    json_set(rargs, "encoding", json_string("utf-8"));
    json_set(rargs, "errors", json_string("replace"));
    json_set(rargs, "timeout", json_number((double)timeout));
    json_set(rargs, "check", json_bool(false));
    json_t *r = ka_seam(io, "subprocess.run", rargs);
    if (ka_raised(r)) {
        /* except Exception -> None; BaseException kinds pass through */
        if (ka_exc_isinstance(json_get_str(r, "__raise__", ""), "Exception")) {
            json_free(r);
            return json_null();
        }
        return r;
    }
    /* result.returncode / result.stdout — SimpleNamespace double */
    if (!r || r->type != JSON_OBJECT || !json_has(r, "returncode")) {
        json_t *e = ka_raise("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'returncode'");
        json_free(r);
        return e;
    }
    json_t *rc = json_obj_get(r, "returncode");
    if (!rc || rc->type != JSON_NUMBER || rc->num_val != 0) {
        json_free(r);
        return json_null();
    }
    const char *so = json_get_str(r, "stdout", "");
    char *stripped = ka_strip(so ? so : "");
    json_free(r);
    if (!stripped || stripped[0] == '\0') {
        free(stripped);
        return json_null();
    }
    json_t *out = json_string(stripped);
    free(stripped);
    return out;
}

/* PoP: ka_project_branch_name @ hermes_cli/kanban_db.py:_project_branch_name */
json_t *ka_project_branch_name(ka_io_t *io, const json_t *project_obj, const char *task_id,
                               const json_t *title) {
    /* the live import sits OUTSIDE the try: an import failure propagates. */
    json_t *imp = ka_seam(io, "import:hermes_cli.projects_db", json_object());
    if (ka_raised(imp)) return imp;
    json_free(imp);
    /* title or "" */
    const char *t = "";
    if (ka_truthy(title)) {
        if (title->type != JSON_STRING) {
            /* `title or ""` keeps a truthy non-str; branch_name_for is seamed
             * so it is forwarded verbatim (out of domain: fail closed). */
            return ka_raise("TypeError", "title must be a string (out of domain)");
        }
        t = title->str_val ? title->str_val : "";
    }
    json_t *args = json_object();
    json_set(args, "project_obj", project_obj ? json_copy(project_obj) : json_null());
    json_set(args, "task_id", json_string(task_id ? task_id : ""));
    json_set(args, "title", json_string(t));
    json_t *r = ka_seam(io, "branch_name_for", args);
    if (ka_raised(r)) {
        if (ka_exc_isinstance(json_get_str(r, "__raise__", ""), "Exception")) {
            json_free(r);
            return json_null();
        }
        return r;
    }
    return r;
}