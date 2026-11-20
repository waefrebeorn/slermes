/*
 * projects_db_flow.c — FLOW + leaf port of hermes_cli/projects_db.py
 * (per-profile first-class Project store), watercup worst-tier remainder.
 *
 * Self-contained, libjson-only. Every function is static (pdb_*); the harness
 * #includes this file and drives pdb_dispatch through the universal seam:
 *   io->call(io, name, args) returns a NEW json_t the CALLER frees.
 *   NULL == Python None. {"__raise__": cls, "message": msg} == an exception.
 *   args is OWNED by pdb_seam (copied into the ordered trace event, then
 *   freed). io->trace / io->state belong to the harness. Borrowed inputs are
 *   never mutated or freed.
 *
 * PoP grep over src/ + include/ (projects_db.py:<fn> annotations): the REAL
 * sqlite-paradigm CRUD siblings (create_project, list_projects, get_project,
 * update_project, add_folder, remove_folder, set_primary, _set_primary_locked,
 * archive/restore/delete_project, set_active, get_active_id, _unique_slug,
 * record/list_discovered_repos, project_for_path, branch_name_for,
 * normalize_slug, _normalize_path) stay CLAIMED and are SEAMED where a ported
 * flow reaches them (normalize_path, list_projects). This file ports the 19
 * UNCLAIMED + sentinel/partial-claimed worst-tier targets:
 *   projects_db_path, _slugify, _now, connect, _initialize (nested closure),
 *   connect_closing, ProjectFolder.to_dict, Project.to_dict, _load_project,
 *   _primary_path_key, find_by_primary_path, _execute_rowcount,
 *   _upsert_meta_locked, _get_meta, _clear_repos_locked,
 *   get_discovery_policy_key, reconcile_discovered_repos_policy,
 *   clear_discovered_repos, owns (nested closure in project_for_path).
 * (The tail of src/hermes_cli/projects_db.c claims projects_db_path / _now /
 * connect_closing / get_discovery_policy_key / reconcile / clear_discovered /
 * _primary_path_key with printf/strdup SENTINELS, and projects_db_ports.c
 * claims connect / _slugify / to_dict with partial bodies — no 64-cap, no
 * "project" fallback, no is_primary/added_at — so those claims are re-ported
 * here in the flow paradigm. The faithful projects_db_slugify twin carries no
 * PoP. Static names are TU-local: no link collision with the global pdb_*
 * symbols in projects_db_ports.c.)
 *
 * Seam model:
 *   get_hermes_home {} -> home str (hermes_constants; PathDouble join).
 *   path.resolve {path} -> resolved str (Path.resolve; lexical POSIX).
 *   open_db {path, db_label, foreign_keys, initialize} -> conn token
 *       (sqlite_util). The initialize callback runs INSIDE open_db: the
 *       oracle stub calls the live nested _initialize; the C port composes
 *       pdb_initialize right after the seam (same trace order). An initialize
 *       raise triggers conn.close (open_db's except-BaseException cleanup);
 *       a close raise MASKS the initialize raise.
 *   conn.execute {sql, params} -> cursor spec {fetchone?, fetchall?, rowcount?}
 *       (params is the parameter list; () projects to []). fetchone/fetchall/
 *       rowcount are INERT reads on the spec (no extra events); a missing key
 *       names the double: AttributeError "'CursorDouble' object has no
 *       attribute '<key>'". fetchone null == no row; rows are NON-EMPTY
 *       objects (sqlite3.Row doubles: row["k"] missing -> IndexError "No item
 *       with that key"; row.keys() == JSON key order).
 *   conn.executescript {sql} / conn.close {} -> null.
 *   add_column_if_missing {table, column, ddl} -> bool (sqlite_util; the conn
 *       arg is dropped — single-conn domain).
 *   normalize_path {path} -> str (CLAIMED _normalize_path sibling).
 *   list_projects {include_archived} -> [project field dicts] (CLAIMED
 *       sibling; conn dropped). Elements materialize as SimpleNamespace
 *       doubles (folders elements too); a missing attribute names the double.
 *   time.time {} -> number (_now's int(time.time())).
 *   write_txn (sqlite_util) is STRUCTURAL: its BEGIN IMMEDIATE / COMMIT /
 *       ROLLBACK conn.execute calls are seams and the CM logic is transcribed
 *       (except Exception -> ROLLBACK, suppress sqlite3.OperationalError on
 *       the ROLLBACK, re-raise the body exception; a non-OperationalError
 *       ROLLBACK raise MASKS the body raise; BaseException kinds skip the
 *       ROLLBACK entirely). The FULL builtin exception-parent table below
 *       drives every except Exception / suppress(Exception) /
 *       except OperationalError match so scripted SystemExit /
 *       KeyboardInterrupt / GeneratorExit pass through untouched.
 *
 * Composed ported siblings stay REAL on both sides: connect ->
 * projects_db_path + _initialize; connect_closing -> connect;
 * get_discovery_policy_key -> _get_meta; reconcile / clear_discovered_repos
 * -> _clear_repos_locked -> _upsert_meta_locked; find_by_primary_path ->
 * _primary_path_key; Project.to_dict -> ProjectFolder.to_dict.
 *
 * Fixture domain (documented, enforced by the generator):
 *  - ASCII scope for slug/name grammars (str().strip() honours the full
 *    Python White_Space set; .lower() is ASCII-only — Unicode lowerings that
 *    land in [a-z0-9] are OUT of scope).
 *  - JSON numbers lose int/float identity after the libjson parse: str()/
 *    repr()-position fixtures carry ints or NON-integral floats.
 *  - Paths are lexical POSIX (PurePosixPath): '.'/empty components dropped,
 *    '..' kept, exactly-two leading slashes preserved; the os double carries
 *    sep "/" and normcase = posixpath.normcase (identity on str; non-str ->
 *    TypeError "expected str, bytes or os.PathLike object, not <T>"). The
 *    live Windows host's nt semantics are OUT of the fixture domain.
 *  - Row values: str/int/float/null (bool excluded — sqlite coerces to int);
 *    row keys are SQL identifiers; rows non-empty. Cursor fetchall is a list;
 *    params is a list or null (null -> TypeError, not iterable).
 *  - find_by_primary_path: proj.folders is a list or null; project/folder
 *    doubles are field dicts (missing attr -> double-named AttributeError)
 *    or raw scalars (attr reads name their real type). The to_dict drives
 *    materialize object.__new__ dataclass instances: plain-default fields
 *    (label/is_primary/added_at, the optional TEXT fields, archived) read
 *    their CLASS DEFAULTS when absent; required fields (path, id/slug/name/
 *    created_at) and default_factory folders raise the class-named
 *    AttributeError. The C mirrors those defaults for dict carriers.
 *  - _slugify name / normalize_slug-free domain: str|null|int|bool|
 *    NON-integral float. time.time scripts: number|bool|null.
 *  - Only table-known exception kinds are scripted (the hierarchy table
 *    fails closed on unknowns).
 *
 * Fail-closed: a missing seam channel, or a missing/exhausted script in the
 * harness backend, raises RuntimeError through the same envelope. out.state
 * is {"initialized_paths": sorted} (_INITIALIZED_PATHS module set; the
 * harness seeds it from in.state and sorts on echo; the oracle seeds the
 * live set per case and echoes sorted).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── live module constants (byte-pinned to projects_db.py; the generator's
 * --check-const verifies SCHEMA_SQL / tuples / meta keys against the AST) ── */
static const char PDB_SCHEMA_SQL[] =
"\n"
"CREATE TABLE IF NOT EXISTS projects (\n"
"    id            TEXT PRIMARY KEY,\n"
"    slug          TEXT NOT NULL UNIQUE,\n"
"    name          TEXT NOT NULL,\n"
"    description   TEXT,\n"
"    icon          TEXT,\n"
"    color         TEXT,\n"
"    board_slug    TEXT,\n"
"    primary_path  TEXT,\n"
"    created_at    INTEGER NOT NULL,\n"
"    archived      INTEGER NOT NULL DEFAULT 0\n"
");\n"
"\n"
"CREATE TABLE IF NOT EXISTS project_folders (\n"
"    project_id  TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,\n"
"    path        TEXT NOT NULL,\n"
"    label       TEXT,\n"
"    is_primary  INTEGER NOT NULL DEFAULT 0,\n"
"    added_at    INTEGER NOT NULL,\n"
"    PRIMARY KEY (project_id, path)\n"
");\n"
"\n"
"CREATE INDEX IF NOT EXISTS idx_project_folders_path\n"
"    ON project_folders(path);\n"
"\n"
"CREATE TABLE IF NOT EXISTS project_meta (\n"
"    key    TEXT PRIMARY KEY,\n"
"    value  TEXT\n"
");\n"
"\n"
"-- Git repos found by scanning the filesystem (desktop \"repo-first\" discovery).\n"
"-- Cached here so the overview is instant after the first scan instead of\n"
"-- re-walking the disk every time the Projects view opens.\n"
"CREATE TABLE IF NOT EXISTS discovered_repos (\n"
"    root          TEXT PRIMARY KEY,\n"
"    label         TEXT,\n"
"    last_seen     INTEGER NOT NULL\n"
");\n";

static const char *PDB_OPTIONAL_PROJECT_COLUMNS[] = {
    "board_slug", "primary_path", "icon", "color"
};
static const char *PDB_OPTIONAL_ROW_FIELDS[] = {
    "description", "icon", "color", "board_slug", "primary_path"
};
#define PDB_ACTIVE_META_KEY "active_id"
#define PDB_DISCOVERY_POLICY_META_KEY "repo_discovery_policy"

/* ── seam dispatcher + ordered trace ─────────────────────────────────────── */
typedef struct pdb_io_t {
    json_t *(*call)(struct pdb_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} (harness-owned) */
    json_t *state;   /* {"initialized_paths":[...]} (harness-owned) */
    void   *ctx;     /* harness backend (scripted results) */
} pdb_io_t;

static bool pdb_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *pdb_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *pdb_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    return pdb_raise(t, m);
}

/* ── FULL builtin exception hierarchy (CPython 3.14.9, probe-verified against
 * the quarry interpreter: every builtins name subclassing BaseException, plus
 * the sqlite3 exception family). Aliases EnvironmentError/IOError/WindowsError
 * ARE OSError (is-identity verified). ExceptionGroup has the secondary parent
 * Exception. Used for except Exception / suppress(Exception) /
 * except sqlite3.OperationalError matching so scripted BaseException kinds
 * pass through untouched. ─────────────────────────────────────────────────── */
typedef struct { const char *name, *parent, *parent2; } pdb_exc_entry_t;
static const pdb_exc_entry_t PDB_EXC_TABLE[] = {
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
};
static const char *pdb_exc_canon(const char *t) {
    if (strcmp(t, "EnvironmentError") == 0 || strcmp(t, "IOError") == 0 ||
        strcmp(t, "WindowsError") == 0)
        return "OSError";
    return t;
}
static const pdb_exc_entry_t *pdb_exc_find(const char *t) {
    t = pdb_exc_canon(t);
    for (size_t i = 0; i < sizeof(PDB_EXC_TABLE)/sizeof(PDB_EXC_TABLE[0]); i++)
        if (strcmp(PDB_EXC_TABLE[i].name, t) == 0) return &PDB_EXC_TABLE[i];
    return NULL;
}
/* isinstance(exc_type, parent) over the table; unknown types fail closed. */
static bool pdb_exc_isinstance(const char *type, const char *parent) {
    if (!type || !parent) return false;
    type = pdb_exc_canon(type);
    parent = pdb_exc_canon(parent);
    if (strcmp(type, parent) == 0) return true;
    const pdb_exc_entry_t *e = pdb_exc_find(type);
    int guard = 0;
    while (e && guard++ < 64) {
        if (e->parent && strcmp(e->parent, parent) == 0) return true;
        if (e->parent2 && strcmp(e->parent2, parent) == 0) return true;
        if (!e->parent) return false;
        e = pdb_exc_find(e->parent);
    }
    return false;
}

/* Record {"call":name,**args}, dispatch, consume args. Fail-closed without a
 * channel. The result (value or raise spec) is caller-owned. */
static json_t *pdb_seam(pdb_io_t *io, const char *name, json_t *args) {
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
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    if (args) json_free(args);
    if (!r) {
        if (!io || !io->call)
            return pdb_raise("RuntimeError", "no seam channel");
        return json_null();
    }
    return r;
}

/* ── Python-faithful micro helpers ───────────────────────────────────────── */
static bool pdb_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool pdb_truthy(const json_t *v) {
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
static const char *pdb_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *pdb_cp(const json_t *v) { return v ? json_copy(v) : json_null(); }
static char *pdb_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *pdb_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return pdb_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : pdb_strdup("");
}
/* Python type(v).__name__ for a JSON node (numbers: int when integral). */
static const char *pdb_type_name(const json_t *v) {
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
static json_t *pdb_attr_error(const json_t *v, const char *attr) {
    char *msg = pdb_asprintf("'%s' object has no attribute '%s'", pdb_type_name(v), attr);
    json_t *e = pdb_raise("AttributeError", msg);
    free(msg);
    return e;
}
static json_t *pdb_iter_error(const json_t *v) {
    char *msg = pdb_asprintf("'%s' object is not iterable", pdb_type_name(v));
    json_t *e = pdb_raise("TypeError", msg);
    free(msg);
    return e;
}
/* Attribute read on a double: a dict carrier missing the key names the DOUBLE
 * class (cls); a non-dict carrier names its real JSON type. */
static json_t *pdb_attr_get(const json_t *obj, const char *key, const char *cls) {
    if (obj && obj->type == JSON_OBJECT) {
        if (json_has(obj, key)) return json_copy(json_obj_get(obj, key));
        char *msg = pdb_asprintf("'%s' object has no attribute '%s'", cls, key);
        json_t *e = pdb_raise("AttributeError", msg);
        free(msg);
        return e;
    }
    return pdb_attr_error(obj, key);
}
/* Dataclass-default attribute read: a dict carrier missing the key reads the
 * CLASS DEFAULT (plain-default fields are class attributes on an
 * object.__new__ instance; default_factory fields are NOT — those stay
 * required via pdb_attr_get). defval is consumed. Non-dict carriers still
 * name their real type. */
static json_t *pdb_attr_get_opt(const json_t *obj, const char *key, const char *cls,
                                json_t *defval) {
    (void)cls;
    if (obj && obj->type == JSON_OBJECT) {
        if (json_has(obj, key)) {
            json_free(defval);
            return json_copy(json_obj_get(obj, key));
        }
        return defval;
    }
    json_free(defval);
    return pdb_attr_error(obj, key);
}
/* str equality (live == between a str and any other JSON scalar is False). */
static bool pdb_str_eq(const json_t *a, const char *b) {
    const char *as = pdb_str(a);
    return as && b && strcmp(as, b) == 0;
}
/* Scalar equality for the reconcile current==policy_key gate (str|null). */
static bool pdb_scalar_eq(const json_t *a, const json_t *b) {
    if (pdb_is_none(a) && pdb_is_none(b)) return true;
    if (pdb_is_none(a) || pdb_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        default: return false;
    }
}

/* UTF-8 code point length (Python len(str)). */
static size_t pdb_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static unsigned long pdb_next_cp(const unsigned char **p) {
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
static bool pdb_is_space_cp(unsigned long cp) {
    if (cp <= 0x7F)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* Python str.strip() (both ends). Caller frees. */
static char *pdb_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = pdb_next_cp(&q);
        if (!pdb_is_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = pdb_next_cp(&r);
        if (!pdb_is_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}
/* str.strip(chars) for an ASCII char set (the "-_" slug strips). Caller frees. */
static char *pdb_strip_chars(const char *s, const char *chars) {
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(chars, s[a])) a++;
    while (b > a && strchr(chars, s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* str.rstrip(chars) for an ASCII char set. Caller frees. */
static char *pdb_rstrip_chars(const char *s, const char *chars) {
    size_t b = strlen(s);
    while (b > 0 && strchr(chars, s[b - 1])) b--;
    char *out = malloc(b + 1);
    if (!out) return NULL;
    memcpy(out, s, b);
    out[b] = '\0';
    return out;
}
/* ASCII-only .lower() (fixture domain: ASCII; see header). Caller frees. */
static char *pdb_lower_ascii(const char *s) {
    char *out = pdb_strdup(s ? s : "");
    if (out)
        for (char *p = out; *p; p++)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return out;
}

/* ── string builder ───────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } pdb_sb_t;
static void pdb_sb_init(pdb_sb_t *sb) { sb->p = NULL; sb->len = 0; sb->cap = 0; }
static void pdb_sb_addn(pdb_sb_t *sb, const char *s, size_t n) {
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
static void pdb_sb_add(pdb_sb_t *sb, const char *s) { pdb_sb_addn(sb, s, strlen(s)); }
static void pdb_sb_free(pdb_sb_t *sb) { free(sb->p); sb->p = NULL; sb->len = sb->cap = 0; }

/* ── Python str(number) / repr(number) ───────────────────────────────────── */
/* Shortest round-trip float repr, formatted like Python repr(float). */
static void pdb_fmt_float(pdb_sb_t *sb, double v) {
    char buf[64];
    if (isnan(v)) { pdb_sb_add(sb, "NaN"); return; }
    if (isinf(v)) { pdb_sb_add(sb, v < 0 ? "-Infinity" : "Infinity"); return; }
    if (v == 0.0) { pdb_sb_add(sb, signbit(v) ? "-0.0" : "0.0"); return; }
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
    if (neg) pdb_sb_add(sb, "-");
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t intd = (size_t)exp + 1;
            if (nd <= intd) {
                pdb_sb_add(sb, digits);
                for (size_t i = nd; i < intd; i++) pdb_sb_add(sb, "0");
                pdb_sb_add(sb, ".0");
            } else {
                char tmp[64];
                memcpy(tmp, digits, intd);
                tmp[intd] = '\0';
                pdb_sb_add(sb, tmp);
                pdb_sb_add(sb, ".");
                pdb_sb_add(sb, digits + intd);
            }
        } else {
            pdb_sb_add(sb, "0.");
            for (int i = 0; i < -exp - 1; i++) pdb_sb_add(sb, "0");
            pdb_sb_add(sb, digits);
        }
    } else {
        char tmp[8];
        tmp[0] = digits[0]; tmp[1] = '\0';
        pdb_sb_add(sb, tmp);
        if (nd > 1) {
            pdb_sb_add(sb, ".");
            pdb_sb_add(sb, digits + 1);
        }
        snprintf(buf, sizeof buf, "e%c%02d", exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
        pdb_sb_add(sb, buf);
    }
}
/* Python str() of a JSON number (integral -> int form). */
static void pdb_fmt_number(pdb_sb_t *sb, double v) {
    if (v == trunc(v) && fabs(v) < 9.2e18) {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", (long long)v);
        pdb_sb_add(sb, buf);
    } else {
        pdb_fmt_float(sb, v);
    }
}
/* Python str(v) for the scalar fixture domain (str|int|float|bool|null).
 * Returns a malloc'd string, or NULL with *err set (containers: out of
 * domain, fail closed). NULL input models None -> "None". */
static char *pdb_py_str_node(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) return pdb_strdup("None");
    if (v->type == JSON_STRING) return pdb_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return pdb_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        pdb_sb_t sb; pdb_sb_init(&sb);
        pdb_fmt_number(&sb, v->num_val);
        return sb.p ? sb.p : pdb_strdup("");
    }
    *err = pdb_raise("TypeError", "str() of a container is out of the fixture domain");
    return NULL;
}

/* ── lexical POSIX paths (PurePosixPath) ──────────────────────────────────── */
typedef struct { char *root; char **parts; size_t n, cap; } pdb_path_t;
static void pdb_path_init(pdb_path_t *p) { p->root = pdb_strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }
static void pdb_path_push(pdb_path_t *p, const char *s, size_t n) {
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
static void pdb_path_parse(pdb_path_t *p, const char *s) {
    pdb_path_init(p);
    const char *q = s ? s : "";
    size_t slashes = 0;
    while (q[slashes] == '/') slashes++;
    if (slashes == 2) { free(p->root); p->root = pdb_strdup("//"); }
    else if (slashes >= 1) { free(p->root); p->root = pdb_strdup("/"); }
    const char *seg = q;
    for (;;) {
        const char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        pdb_path_push(p, seg, len);
        if (!slash) break;
        seg = slash + 1;
    }
}
static void pdb_path_free(pdb_path_t *p) {
    free(p->root);
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = p->cap = 0;
}
static char *pdb_path_str(const pdb_path_t *p) {
    pdb_sb_t sb;
    pdb_sb_init(&sb);
    pdb_sb_add(&sb, p->root);
    for (size_t i = 0; i < p->n; i++) {
        if (i) pdb_sb_add(&sb, "/");
        pdb_sb_add(&sb, p->parts[i]);
    }
    if (sb.len == 0) pdb_sb_add(&sb, ".");
    return sb.p ? sb.p : pdb_strdup(".");
}
/* Normalised str(Path(s)) — caller frees. */
static char *pdb_path_norm(const char *s) {
    pdb_path_t p;
    pdb_path_parse(&p, s);
    char *out = pdb_path_str(&p);
    pdb_path_free(&p);
    return out;
}

/* ── sqlite3.Row / cursor double reads ───────────────────────────────────── */
/* row["key"]: sqlite3.Row raises IndexError "No item with that key". */
static json_t *pdb_row_get(const json_t *row, const char *key) {
    if (row && row->type == JSON_OBJECT && json_has(row, key))
        return json_copy(json_obj_get(row, key));
    return pdb_raise("IndexError", "No item with that key");
}
/* Row double materialization check (fetchone/fetchall element): a non-empty
 * object. Mirrors the oracle's make_row fail-closed domain. */
static json_t *pdb_row_check(const json_t *row) {
    if (!row || row->type != JSON_OBJECT)
        return pdb_raise("TypeError", "row double must be an object");
    if (row->c.count == 0)
        return pdb_raise("ValueError", "row double must be non-empty");
    return NULL;
}
/* Cursor spec check: an object (the CursorDouble materialization domain). */
static json_t *pdb_cursor_check(const json_t *spec) {
    if (!spec || spec->type != JSON_OBJECT)
        return pdb_raise("TypeError", "cursor double must be an object");
    return NULL;
}
static json_t *pdb_cursor_attr_error(const char *key) {
    char *msg = pdb_asprintf("'CursorDouble' object has no attribute '%s'", key);
    json_t *e = pdb_raise("AttributeError", msg);
    free(msg);
    return e;
}
/* cur.fetchone() -> row (caller-owned) or NULL (None); raise spec on error. */
static json_t *pdb_cursor_fetchone(const json_t *spec, json_t **err) {
    *err = NULL;
    json_t *ce = pdb_cursor_check(spec);
    if (ce) { *err = ce; return NULL; }
    if (!json_has(spec, "fetchone")) { *err = pdb_cursor_attr_error("fetchone"); return NULL; }
    json_t *r = json_obj_get(spec, "fetchone");
    if (pdb_is_none(r)) return NULL;
    json_t *re = pdb_row_check(r);
    if (re) { *err = re; return NULL; }
    return json_copy(r);
}
/* cur.fetchall() -> array of rows (caller-owned); raise spec on error. */
static json_t *pdb_cursor_fetchall(const json_t *spec, json_t **err) {
    *err = NULL;
    json_t *ce = pdb_cursor_check(spec);
    if (ce) { *err = ce; return NULL; }
    if (!json_has(spec, "fetchall")) { *err = pdb_cursor_attr_error("fetchall"); return NULL; }
    json_t *rows = json_obj_get(spec, "fetchall");
    if (!rows || rows->type != JSON_ARRAY) { *err = pdb_iter_error(rows); return NULL; }
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *re = pdb_row_check(rows->c.items[i]);
        if (re) { json_free(out); *err = re; return NULL; }
        json_append(out, json_copy(rows->c.items[i]));
    }
    return out;
}
/* cur.rowcount -> value (caller-owned); raise spec on error. */
static json_t *pdb_cursor_rowcount(const json_t *spec, json_t **err) {
    *err = NULL;
    json_t *ce = pdb_cursor_check(spec);
    if (ce) { *err = ce; return NULL; }
    if (!json_has(spec, "rowcount")) { *err = pdb_cursor_attr_error("rowcount"); return NULL; }
    return json_copy(json_obj_get(spec, "rowcount"));
}

/* ── conn seams (single-conn domain: the conn token never rides in args) ─── */
static json_t *pdb_conn_execute(pdb_io_t *io, const char *sql, json_t *params) {
    json_t *a = json_object();
    json_set(a, "sql", json_string(sql));
    json_set(a, "params", params ? params : json_array());
    json_t *r = pdb_seam(io, "conn.execute", a);
    if (pdb_raised(r)) return r;
    /* live materializes CursorDouble(spec) AT execute time: a non-object
     * spec raises inside the caller's frame (inside write_txn -> ROLLBACK),
     * not at the later fetchone/fetchall/rowcount read. */
    json_t *ce = pdb_cursor_check(r);
    if (ce) { json_free(r); return ce; }
    return r;
}
static json_t *pdb_conn_executescript(pdb_io_t *io, const char *sql) {
    json_t *a = json_object();
    json_set(a, "sql", json_string(sql));
    return pdb_seam(io, "conn.executescript", a);
}
static json_t *pdb_conn_close(pdb_io_t *io) {
    return pdb_seam(io, "conn.close", json_object());
}

/* ── write_txn (sqlite_util) — STRUCTURAL transcription ──────────────────── */
typedef json_t *(*pdb_txn_body_fn)(pdb_io_t *io, void *ctx);
/* BEGIN IMMEDIATE; body; except Exception -> ROLLBACK (suppress
 * OperationalError on the ROLLBACK itself) + re-raise; else COMMIT.
 * BaseException body kinds skip the ROLLBACK. A non-OperationalError ROLLBACK
 * raise masks the body raise; a COMMIT raise discards the body value. */
static json_t *pdb_write_txn(pdb_io_t *io, pdb_txn_body_fn body, void *ctx) {
    json_t *begin = pdb_conn_execute(io, "BEGIN IMMEDIATE", json_array());
    if (pdb_raised(begin)) return begin;
    json_free(begin);
    json_t *r = body(io, ctx);
    if (pdb_raised(r)) {
        const char *kind = json_get_str(r, "__raise__", "");
        if (pdb_exc_isinstance(kind, "Exception")) {
            json_t *rb = pdb_conn_execute(io, "ROLLBACK", json_array());
            if (pdb_raised(rb)) {
                const char *rk = json_get_str(rb, "__raise__", "");
                if (pdb_exc_isinstance(rk, "OperationalError")) {
                    json_free(rb);
                    return r;
                }
                json_free(r);
                return rb;
            }
            json_free(rb);
        }
        return r;
    }
    json_t *commit = pdb_conn_execute(io, "COMMIT", json_array());
    if (pdb_raised(commit)) { json_free(r); return commit; }
    json_free(commit);
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PoP-ported functions (19 UNCLAIMED + sentinel/partial-claimed targets)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* PoP: pdb_projects_db_path @ hermes_cli/projects_db.py:projects_db_path */
static json_t *pdb_projects_db_path(pdb_io_t *io) {
    /* return get_hermes_home() / "projects.db" */
    json_t *home = pdb_seam(io, "get_hermes_home", json_object());
    if (pdb_raised(home)) return home;
    const char *hs = pdb_str(home);
    if (!hs) {
        char *msg = pdb_asprintf(
            "argument should be a str or an os.PathLike object where __fspath__ "
            "returns a str, not '%s'", pdb_type_name(home));
        json_t *e = pdb_raise("TypeError", msg);
        free(msg);
        json_free(home);
        return e;
    }
    pdb_path_t p;
    pdb_path_parse(&p, hs);
    pdb_path_push(&p, "projects.db", strlen("projects.db"));
    char *s = pdb_path_str(&p);
    pdb_path_free(&p);
    json_free(home);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: pdb_slugify @ hermes_cli/projects_db.py:_slugify */
static json_t *pdb_slugify(pdb_io_t *io, const json_t *name) {
    (void)io;
    /* s = re.sub(r"[^a-z0-9]+", "-", str(name or "").strip().lower()).strip("-_") */
    char *raw = NULL;
    if (pdb_truthy(name)) {
        json_t *err = NULL;
        raw = pdb_py_str_node(name, &err);
        if (!raw) return err;
    } else {
        raw = pdb_strdup("");
    }
    char *stripped = pdb_strip(raw);
    free(raw);
    char *low = pdb_lower_ascii(stripped ? stripped : "");
    free(stripped);
    /* re.sub: runs of non-[a-z0-9] collapse to a single '-' */
    pdb_sb_t sb; pdb_sb_init(&sb);
    bool in_sep = false;
    for (const char *q = low; *q; q++) {
        unsigned char c = (unsigned char)*q;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            pdb_sb_addn(&sb, (const char *)&c, 1);
            in_sep = false;
        } else if (!in_sep) {
            pdb_sb_add(&sb, "-");
            in_sep = true;
        }
    }
    free(low);
    char *s1 = pdb_strip_chars(sb.p ? sb.p : "", "-_");
    pdb_sb_free(&sb);
    /* return s[:64].strip("-_") or "project"  (post-sub text is ASCII) */
    char *cap = pdb_strdup(s1 ? s1 : "");
    free(s1);
    if (strlen(cap) > 64) cap[64] = '\0';
    char *s2 = pdb_strip_chars(cap, "-_");
    free(cap);
    json_t *r = json_string((s2 && s2[0]) ? s2 : "project");
    free(s2);
    return r;
}

/* PoP: pdb_now @ hermes_cli/projects_db.py:_now */
static json_t *pdb_now(pdb_io_t *io) {
    /* return int(time.time()) */
    json_t *t = pdb_seam(io, "time.time", json_object());
    if (pdb_raised(t)) return t;
    double v = 0.0;
    if (pdb_is_none(t)) {
        json_free(t);
        return pdb_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    }
    if (t->type == JSON_BOOL) {
        v = t->bool_val ? 1.0 : 0.0;
    } else if (t->type == JSON_NUMBER) {
        v = trunc(t->num_val);
    } else {
        char *msg = pdb_asprintf(
            "int() argument must be a string, a bytes-like object or a real number, not '%s'",
            pdb_type_name(t));
        json_t *e = pdb_raise("TypeError", msg);
        free(msg);
        json_free(t);
        return e;
    }
    json_free(t);
    return json_number(v);
}

/* PoP: pdb_initialize @ hermes_cli/projects_db.py:_initialize */
static json_t *pdb_initialize(pdb_io_t *io, const json_t *conn, const char *resolved) {
    (void)conn;
    /* if resolved in _INITIALIZED_PATHS: return */
    json_t *paths = io->state ? json_obj_get(io->state, "initialized_paths") : NULL;
    if (paths && paths->type == JSON_ARRAY) {
        for (size_t i = 0; i < paths->c.count; i++)
            if (pdb_str_eq(paths->c.items[i], resolved)) return json_null();
    }
    /* conn.executescript(SCHEMA_SQL) */
    json_t *es = pdb_conn_executescript(io, PDB_SCHEMA_SQL);
    if (pdb_raised(es)) return es;
    json_free(es);
    /* cols = {row["name"] for row in conn.execute("PRAGMA table_info(projects)")} */
    json_t *cur = pdb_conn_execute(io, "PRAGMA table_info(projects)", json_array());
    if (pdb_raised(cur)) return cur;
    json_t *err = NULL;
    json_t *rows = pdb_cursor_fetchall(cur, &err);
    json_free(cur);
    if (err) return err;
    json_t *cols = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *nm = pdb_row_get(rows->c.items[i], "name");
        if (pdb_raised(nm)) { json_free(rows); json_free(cols); return nm; }
        bool dup = false;
        for (size_t j = 0; j < cols->c.count; j++)
            if (pdb_scalar_eq(cols->c.items[j], nm)) { dup = true; break; }
        if (!dup) json_append(cols, nm);
        else json_free(nm);
    }
    json_free(rows);
    /* for col in _OPTIONAL_PROJECT_COLUMNS: if col not in cols: add_column */
    for (size_t k = 0; k < sizeof(PDB_OPTIONAL_PROJECT_COLUMNS)/sizeof(PDB_OPTIONAL_PROJECT_COLUMNS[0]); k++) {
        const char *col = PDB_OPTIONAL_PROJECT_COLUMNS[k];
        bool has = false;
        for (size_t j = 0; j < cols->c.count; j++)
            if (pdb_str_eq(cols->c.items[j], col)) { has = true; break; }
        if (has) continue;
        json_t *a = json_object();
        json_set(a, "table", json_string("projects"));
        json_set(a, "column", json_string(col));
        char *ddl = pdb_asprintf("%s TEXT", col);
        json_set(a, "ddl", json_string(ddl));
        free(ddl);
        json_t *r = pdb_seam(io, "add_column_if_missing", a);
        if (pdb_raised(r)) { json_free(cols); return r; }
        json_free(r);
    }
    json_free(cols);
    /* _INITIALIZED_PATHS.add(resolved) */
    if (io->state) {
        json_t *arr = json_obj_get(io->state, "initialized_paths");
        if (!arr || arr->type != JSON_ARRAY) {
            arr = json_array();
            json_set(io->state, "initialized_paths", arr);
        }
        json_append(arr, json_string(resolved));
    }
    return json_null();
}

/* PoP: pdb_connect @ hermes_cli/projects_db.py:connect */
static json_t *pdb_connect(pdb_io_t *io, const json_t *db_path) {
    /* path = db_path if db_path is not None else projects_db_path() */
    char *path_str = NULL;
    if (pdb_is_none(db_path)) {
        json_t *hp = pdb_projects_db_path(io);
        if (pdb_raised(hp)) return hp;
        path_str = pdb_strdup(pdb_str(hp) ? pdb_str(hp) : "");
        json_free(hp);
    } else if (db_path->type == JSON_STRING) {
        path_str = pdb_path_norm(db_path->str_val);
    } else {
        /* path.resolve() on a non-Path carrier names the OUTER attribute */
        return pdb_attr_error(db_path, "resolve");
    }
    /* resolved = str(path.resolve()) */
    json_t *ra = json_object();
    json_set(ra, "path", json_string(path_str));
    json_t *resolved = pdb_seam(io, "path.resolve", ra);
    if (pdb_raised(resolved)) { free(path_str); return resolved; }
    const char *res_s = pdb_str(resolved);
    if (!res_s) {
        char *msg = pdb_asprintf(
            "argument should be a str or an os.PathLike object where __fspath__ "
            "returns a str, not '%s'", pdb_type_name(resolved));
        json_t *e = pdb_raise("TypeError", msg);
        free(msg);
        free(path_str);
        json_free(resolved);
        return e;
    }
    /* open_db(path, db_label="projects.db", foreign_keys=True, initialize=_initialize) */
    json_t *oa = json_object();
    json_set(oa, "path", json_string(path_str));
    json_set(oa, "db_label", json_string("projects.db"));
    json_set(oa, "foreign_keys", json_bool(true));
    json_set(oa, "initialize", json_bool(true));
    json_t *conn = pdb_seam(io, "open_db", oa);
    free(path_str);
    if (pdb_raised(conn)) { json_free(resolved); return conn; }
    /* initialize runs INSIDE open_db (the oracle stub calls the live closure);
     * the C composes it here — same trace order. open_db's except-BaseException
     * cleanup closes the conn on an initialize raise; a close raise MASKS it. */
    json_t *ir = pdb_initialize(io, conn, res_s);
    if (pdb_raised(ir)) {
        json_t *cl = pdb_conn_close(io);
        json_free(resolved);
        json_free(conn);
        if (pdb_raised(cl)) { json_free(ir); return cl; }
        json_free(cl);
        return ir;
    }
    json_free(ir);
    json_free(resolved);
    return conn;
}

/* PoP: pdb_connect_closing @ hermes_cli/projects_db.py:connect_closing */
static json_t *pdb_connect_closing(pdb_io_t *io, const json_t *db_path, const json_t *body) {
    /* conn = connect(db_path=db_path); try: yield conn; finally:
     * with contextlib.suppress(Exception): conn.close() */
    json_t *conn = pdb_connect(io, db_path);
    if (pdb_raised(conn)) return conn;
    json_t *body_exc = NULL;
    if (body && body->type == JSON_OBJECT) {
        json_t *spec = json_obj_get(body, "raise");
        if (spec && spec->type == JSON_OBJECT) body_exc = pdb_raise_from_spec(spec);
    }
    json_t *cl = pdb_conn_close(io);
    if (pdb_raised(cl)) {
        const char *kind = json_get_str(cl, "__raise__", "");
        if (pdb_exc_isinstance(kind, "Exception")) {
            json_free(cl);   /* suppress(Exception) */
        } else {
            json_free(conn);
            if (body_exc) json_free(body_exc);
            return cl;       /* BaseException close raise masks the body raise */
        }
    } else {
        json_free(cl);
    }
    json_free(conn);
    if (body_exc) return body_exc;
    return json_null();
}

/* PoP: pdb_folder_to_dict @ hermes_cli/projects_db.py:to_dict */
static json_t *pdb_folder_to_dict(pdb_io_t *io, const json_t *self, const char *cls) {
    (void)io;
    /* {"path": self.path, "label": self.label,
     *  "is_primary": bool(self.is_primary), "added_at": self.added_at}
     * path is required; label/is_primary/added_at carry dataclass class
     * defaults (None/False/0) on a partial instance. */
    json_t *path = pdb_attr_get(self, "path", cls);
    if (pdb_raised(path)) return path;
    json_t *label = pdb_attr_get_opt(self, "label", cls, json_null());
    if (pdb_raised(label)) { json_free(path); return label; }
    json_t *isp = pdb_attr_get_opt(self, "is_primary", cls, json_bool(false));
    if (pdb_raised(isp)) { json_free(path); json_free(label); return isp; }
    json_t *added = pdb_attr_get_opt(self, "added_at", cls, json_number(0));
    if (pdb_raised(added)) { json_free(path); json_free(label); json_free(isp); return added; }
    json_t *out = json_object();
    json_set(out, "path", path);
    json_set(out, "label", label);
    json_set(out, "is_primary", json_bool(pdb_truthy(isp)));
    json_free(isp);
    json_set(out, "added_at", added);
    return out;
}

/* PoP: pdb_project_to_dict @ hermes_cli/projects_db.py:to_dict */
static json_t *pdb_project_to_dict(pdb_io_t *io, const json_t *self) {
    /* d = {k: getattr(self, k) for k in ("id","slug","name",*_OPTIONAL_ROW_FIELDS)}
     * return {**d, "archived": bool(self.archived), "created_at": self.created_at,
     *         "folders": [f.to_dict() for f in self.folders]}
     * id/slug/name/created_at/folders are required (folders' default_factory
     * leaves NO class attribute); the optional TEXT fields and archived carry
     * dataclass class defaults (None/False) on a partial instance. */
    static const char *KEYS[] = {
        "id", "slug", "name", "description", "icon", "color", "board_slug", "primary_path"
    };
    json_t *out = json_object();
    for (size_t i = 0; i < sizeof(KEYS)/sizeof(KEYS[0]); i++) {
        json_t *v = (i < 3) ? pdb_attr_get(self, KEYS[i], "Project")
                            : pdb_attr_get_opt(self, KEYS[i], "Project", json_null());
        if (pdb_raised(v)) { json_free(out); return v; }
        json_set(out, KEYS[i], v);
    }
    json_t *arch = pdb_attr_get_opt(self, "archived", "Project", json_bool(false));
    if (pdb_raised(arch)) { json_free(out); return arch; }
    json_t *created = pdb_attr_get(self, "created_at", "Project");
    if (pdb_raised(created)) { json_free(out); json_free(arch); return created; }
    json_t *folders = pdb_attr_get(self, "folders", "Project");
    if (pdb_raised(folders)) { json_free(out); json_free(arch); json_free(created); return folders; }
    if (!folders || folders->type != JSON_ARRAY) {
        json_t *e = pdb_iter_error(folders);
        json_free(out); json_free(arch); json_free(created); json_free(folders);
        return e;
    }
    json_t *farr = json_array();
    for (size_t i = 0; i < folders->c.count; i++) {
        const json_t *el = folders->c.items[i];
        json_t *fd;
        if (el && el->type == JSON_OBJECT) {
            fd = pdb_folder_to_dict(io, el, "ProjectFolder");
        } else {
            /* f.to_dict() — the OUTER attribute resolves on the raw carrier */
            fd = pdb_attr_error(el, "to_dict");
        }
        if (pdb_raised(fd)) {
            json_free(out); json_free(arch); json_free(created);
            json_free(folders); json_free(farr);
            return fd;
        }
        json_append(farr, fd);
    }
    json_free(folders);
    json_set(out, "archived", json_bool(pdb_truthy(arch)));
    json_free(arch);
    json_set(out, "created_at", created);
    json_set(out, "folders", farr);
    return out;
}

/* PoP: pdb_load_project @ hermes_cli/projects_db.py:_load_project */
static json_t *pdb_load_project(pdb_io_t *io, const json_t *row) {
    /* keys = row.keys(); folders = conn.execute(..., (row["id"],)).fetchall();
     * return Project(id=..., slug=..., name=..., created_at=...,
     *   archived=bool(row["archived"]) if "archived" in keys else False,
     *   folders=[ProjectFolder(r["path"], r["label"], bool(r["is_primary"]),
     *                          r["added_at"]) for r in folders],
     *   **{f: row[f] for f in _OPTIONAL_ROW_FIELDS if f in keys}) */
    json_t *re = pdb_row_check(row);
    if (re) return re;
    /* the folders query runs BEFORE the ctor kwargs; row["id"] is its param */
    json_t *idp = pdb_row_get(row, "id");
    if (pdb_raised(idp)) return idp;
    json_t *params = json_array();
    json_append(params, idp);
    json_t *cur = pdb_conn_execute(io,
        "SELECT path, label, is_primary, added_at FROM project_folders "
        "WHERE project_id = ? ORDER BY is_primary DESC, added_at ASC", params);
    if (pdb_raised(cur)) return cur;
    json_t *err = NULL;
    json_t *frows = pdb_cursor_fetchall(cur, &err);
    json_free(cur);
    if (err) return err;
    /* ctor kwarg evaluation order: id, slug, name, created_at, archived,
     * folders comprehension, optional-fields comprehension. The OUTPUT key
     * order is the dataclass field order (assembled at the end). */
    json_t *idv = pdb_row_get(row, "id");
    if (pdb_raised(idv)) { json_free(frows); return idv; }
    json_t *slug = pdb_row_get(row, "slug");
    if (pdb_raised(slug)) { json_free(frows); json_free(idv); return slug; }
    json_t *name = pdb_row_get(row, "name");
    if (pdb_raised(name)) { json_free(frows); json_free(idv); json_free(slug); return name; }
    json_t *created = pdb_row_get(row, "created_at");
    if (pdb_raised(created)) { json_free(frows); json_free(idv); json_free(slug); json_free(name); return created; }
    bool archived = false;
    if (json_has(row, "archived")) {
        json_t *av = pdb_row_get(row, "archived");
        archived = pdb_truthy(av);
        json_free(av);
    }
    json_t *folders = json_array();
    json_t *fold_raise = NULL;
    for (size_t i = 0; i < frows->c.count; i++) {
        const json_t *fr = frows->c.items[i];
        json_t *fp = pdb_row_get(fr, "path");
        if (pdb_raised(fp)) { fold_raise = fp; break; }
        json_t *fl = pdb_row_get(fr, "label");
        if (pdb_raised(fl)) { json_free(fp); fold_raise = fl; break; }
        json_t *fi = pdb_row_get(fr, "is_primary");
        if (pdb_raised(fi)) { json_free(fp); json_free(fl); fold_raise = fi; break; }
        json_t *fa = pdb_row_get(fr, "added_at");
        if (pdb_raised(fa)) { json_free(fp); json_free(fl); json_free(fi); fold_raise = fa; break; }
        json_t *fo = json_object();
        json_set(fo, "path", fp);
        json_set(fo, "label", fl);
        json_set(fo, "is_primary", json_bool(pdb_truthy(fi)));
        json_free(fi);
        json_set(fo, "added_at", fa);
        json_append(folders, fo);
    }
    json_free(frows);
    if (fold_raise) {
        /* the comprehension is atomic: partial folders are discarded */
        json_free(folders);
        json_free(idv); json_free(slug); json_free(name); json_free(created);
        return fold_raise;
    }
    /* optional-fields comprehension (present keys only; dataclass defaults
     * are None for absent ones) */
    json_t *opt[5];
    for (size_t i = 0; i < 5; i++) {
        const char *f = PDB_OPTIONAL_ROW_FIELDS[i];
        opt[i] = json_has(row, f) ? pdb_row_get(row, f) : json_null();
    }
    /* assemble in dataclass field order */
    json_t *out = json_object();
    json_set(out, "id", idv);
    json_set(out, "slug", slug);
    json_set(out, "name", name);
    json_set(out, "created_at", created);
    for (size_t i = 0; i < 5; i++)
        json_set(out, PDB_OPTIONAL_ROW_FIELDS[i], opt[i]);
    json_set(out, "archived", json_bool(archived));
    json_set(out, "folders", folders);
    return out;
}

/* PoP: pdb_primary_path_key @ hermes_cli/projects_db.py:_primary_path_key */
static json_t *pdb_primary_path_key(pdb_io_t *io, const json_t *path) {
    /* return os.path.normcase(_normalize_path(path)) — _normalize_path is a
     * CLAIMED sibling seam; normcase is the POSIX double (identity on str,
     * os.fspath TypeError on non-str). */
    json_t *a = json_object();
    json_set(a, "path", pdb_cp(path));
    json_t *norm = pdb_seam(io, "normalize_path", a);
    if (pdb_raised(norm)) return norm;
    if (!norm || norm->type != JSON_STRING) {
        char *msg = pdb_asprintf("expected str, bytes or os.PathLike object, not %s",
                                 pdb_type_name(norm));
        json_t *e = pdb_raise("TypeError", msg);
        free(msg);
        json_free(norm);
        return e;
    }
    return norm;
}

/* PoP: pdb_find_by_primary_path @ hermes_cli/projects_db.py:find_by_primary_path */
static json_t *pdb_find_by_primary_path(pdb_io_t *io, const json_t *path, bool include_archived) {
    /* key = _primary_path_key(path)
     * for proj in list_projects(conn, include_archived=...) if key else ():
     *   primary = proj.primary_path or next(
     *       (f.path for f in proj.folders if f.is_primary),
     *       proj.folders[0].path if proj.folders else None)
     *   if primary and _primary_path_key(primary) == key: return proj
     * return None */
    json_t *key = pdb_primary_path_key(io, path);
    if (pdb_raised(key)) return key;
    if (!pdb_truthy(key)) { json_free(key); return json_null(); }
    json_t *la = json_object();
    json_set(la, "include_archived", json_bool(include_archived));
    json_t *projs = pdb_seam(io, "list_projects", la);
    if (pdb_raised(projs)) { json_free(key); return projs; }
    if (!projs || projs->type != JSON_ARRAY) {
        json_t *e = pdb_iter_error(projs);
        json_free(key); json_free(projs);
        return e;
    }
    for (size_t i = 0; i < projs->c.count; i++) {
        const json_t *proj = projs->c.items[i];
        json_t *pp = pdb_attr_get(proj, "primary_path", "types.SimpleNamespace");
        if (pdb_raised(pp)) { json_free(key); json_free(projs); return pp; }
        json_t *primary = NULL;
        if (pdb_truthy(pp)) {
            primary = pp;
        } else {
            json_free(pp);
            /* next(gen, DEFAULT) — the default argument is evaluated EAGERLY,
             * BEFORE the generator iterates: proj.folders[0].path if
             * proj.folders else None. */
            json_t *f0 = pdb_attr_get(proj, "folders", "types.SimpleNamespace");
            if (pdb_raised(f0)) { json_free(key); json_free(projs); return f0; }
            json_t *defv = json_null();
            if (pdb_truthy(f0)) {
                if (f0->type != JSON_ARRAY) {
                    json_t *e = pdb_raise("TypeError", "folders double must be a list or null");
                    json_free(f0); json_free(defv); json_free(key); json_free(projs);
                    return e;
                }
                defv = pdb_attr_get(f0->c.items[0], "path", "types.SimpleNamespace");
                json_free(f0);
                if (pdb_raised(defv)) { json_free(key); json_free(projs); return defv; }
            } else {
                json_free(f0);
            }
            /* then the generator: for f in proj.folders if f.is_primary */
            json_t *f1 = pdb_attr_get(proj, "folders", "types.SimpleNamespace");
            if (pdb_raised(f1)) { json_free(defv); json_free(key); json_free(projs); return f1; }
            if (!f1 || f1->type != JSON_ARRAY) {
                json_t *e = pdb_iter_error(f1);
                json_free(f1); json_free(defv); json_free(key); json_free(projs);
                return e;
            }
            for (size_t j = 0; j < f1->c.count; j++) {
                const json_t *f = f1->c.items[j];
                json_t *isp = pdb_attr_get(f, "is_primary", "types.SimpleNamespace");
                if (pdb_raised(isp)) {
                    json_free(f1); json_free(defv); json_free(key); json_free(projs);
                    return isp;
                }
                bool prim = pdb_truthy(isp);
                json_free(isp);
                if (prim) {
                    json_t *fp = pdb_attr_get(f, "path", "types.SimpleNamespace");
                    if (pdb_raised(fp)) {
                        json_free(f1); json_free(defv); json_free(key); json_free(projs);
                        return fp;
                    }
                    json_free(defv);
                    defv = fp;
                    break;
                }
            }
            json_free(f1);
            primary = defv;   /* first primary path, or the eager default */
        }
        bool match = false;
        if (pdb_truthy(primary)) {
            json_t *k2 = pdb_primary_path_key(io, primary);
            if (pdb_raised(k2)) {
                json_free(primary); json_free(key); json_free(projs);
                return k2;
            }
            match = pdb_str_eq(k2, pdb_str(key));
            json_free(k2);
        }
        json_free(primary);
        if (match) {
            json_t *ret = json_copy(proj);
            json_free(key); json_free(projs);
            return ret;
        }
    }
    json_free(key);
    json_free(projs);
    return json_null();
}

/* _execute_rowcount's write_txn body: cur = conn.execute(sql, params) */
typedef struct { const json_t *sql; const json_t *params; } pdb_er_ctx_t;
static json_t *pdb_er_body(pdb_io_t *io, void *vctx) {
    pdb_er_ctx_t *c = (pdb_er_ctx_t *)vctx;
    /* conn.execute(sql, params) — the double does list(params): a non-list
     * (null included) is not iterable in the fixture domain. */
    if (!c->params || c->params->type != JSON_ARRAY)
        return pdb_iter_error(c->params);
    json_t *a = json_object();
    json_set(a, "sql", pdb_cp(c->sql));
    json_set(a, "params", json_copy(c->params));
    json_t *r = pdb_seam(io, "conn.execute", a);
    if (pdb_raised(r)) return r;
    /* CursorDouble materializes AT execute time (inside the txn body). */
    json_t *ce = pdb_cursor_check(r);
    if (ce) { json_free(r); return ce; }
    return r;
}

/* PoP: pdb_execute_rowcount @ hermes_cli/projects_db.py:_execute_rowcount */
static json_t *pdb_execute_rowcount(pdb_io_t *io, const json_t *sql, const json_t *params) {
    /* with write_txn(conn): cur = conn.execute(sql, params)
     * return cur.rowcount */
    pdb_er_ctx_t ctx = { sql, params };
    json_t *cur = pdb_write_txn(io, pdb_er_body, &ctx);
    if (pdb_raised(cur)) return cur;
    json_t *err = NULL;
    json_t *rc = pdb_cursor_rowcount(cur, &err);
    json_free(cur);
    if (err) return err;
    return rc;
}

/* PoP: pdb_upsert_meta_locked @ hermes_cli/projects_db.py:_upsert_meta_locked */
static json_t *pdb_upsert_meta_locked(pdb_io_t *io, const json_t *key, const json_t *value) {
    /* conn.execute("INSERT INTO project_meta (key, value) VALUES (?, ?)
     *   ON CONFLICT(key) DO UPDATE SET value = excluded.value", (key, value)) */
    json_t *params = json_array();
    json_append(params, pdb_cp(key));
    json_append(params, pdb_cp(value));
    json_t *r = pdb_conn_execute(io,
        "INSERT INTO project_meta (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value", params);
    if (pdb_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: pdb_get_meta @ hermes_cli/projects_db.py:_get_meta */
static json_t *pdb_get_meta(pdb_io_t *io, const json_t *key) {
    /* row = conn.execute("SELECT value FROM project_meta WHERE key = ?",
     *                    (key,)).fetchone()
     * return row["value"] if row else None */
    json_t *params = json_array();
    json_append(params, pdb_cp(key));
    json_t *cur = pdb_conn_execute(io, "SELECT value FROM project_meta WHERE key = ?", params);
    if (pdb_raised(cur)) return cur;
    json_t *err = NULL;
    json_t *row = pdb_cursor_fetchone(cur, &err);
    json_free(cur);
    if (err) return err;
    if (!row) return json_null();
    json_t *v = pdb_row_get(row, "value");
    json_free(row);
    return v;
}

/* PoP: pdb_clear_repos_locked @ hermes_cli/projects_db.py:_clear_repos_locked */
static json_t *pdb_clear_repos_locked(pdb_io_t *io, bool clear, const json_t *policy_key) {
    /* if clear: conn.execute("DELETE FROM discovered_repos")
     * if policy_key is not None:
     *     _upsert_meta_locked(conn, _DISCOVERY_POLICY_META_KEY, policy_key) */
    if (clear) {
        json_t *r = pdb_conn_execute(io, "DELETE FROM discovered_repos", json_array());
        if (pdb_raised(r)) return r;
        json_free(r);
    }
    if (!pdb_is_none(policy_key)) {
        json_t *mk = json_string(PDB_DISCOVERY_POLICY_META_KEY);
        json_t *r = pdb_upsert_meta_locked(io, mk, policy_key);
        json_free(mk);
        return r;
    }
    return json_null();
}

/* PoP: pdb_get_discovery_policy_key @ hermes_cli/projects_db.py:get_discovery_policy_key */
static json_t *pdb_get_discovery_policy_key(pdb_io_t *io) {
    /* return _get_meta(conn, _DISCOVERY_POLICY_META_KEY) */
    json_t *mk = json_string(PDB_DISCOVERY_POLICY_META_KEY);
    json_t *r = pdb_get_meta(io, mk);
    json_free(mk);
    return r;
}

/* reconcile / clear_discovered write_txn body: _clear_repos_locked(...) */
typedef struct { bool clear; const json_t *policy_key; } pdb_cr_ctx_t;
static json_t *pdb_cr_body(pdb_io_t *io, void *vctx) {
    pdb_cr_ctx_t *c = (pdb_cr_ctx_t *)vctx;
    return pdb_clear_repos_locked(io, c->clear, c->policy_key);
}

/* PoP: pdb_reconcile_discovered_repos_policy @ hermes_cli/projects_db.py:reconcile_discovered_repos_policy */
static json_t *pdb_reconcile_discovered_repos_policy(pdb_io_t *io, const json_t *policy_key,
                                                     bool preserve_unversioned) {
    /* current = get_discovery_policy_key(conn)
     * if current == policy_key: return False
     * cleared = current is not None or not preserve_unversioned
     * with write_txn(conn): _clear_repos_locked(conn, cleared, policy_key)
     * return cleared */
    json_t *current = pdb_get_discovery_policy_key(io);
    if (pdb_raised(current)) return current;
    if (pdb_scalar_eq(current, policy_key)) {
        json_free(current);
        return json_bool(false);
    }
    bool cleared = !pdb_is_none(current) || !preserve_unversioned;
    json_free(current);
    pdb_cr_ctx_t ctx = { cleared, policy_key };
    json_t *r = pdb_write_txn(io, pdb_cr_body, &ctx);
    if (pdb_raised(r)) return r;
    json_free(r);
    return json_bool(cleared);
}

/* PoP: pdb_clear_discovered_repos @ hermes_cli/projects_db.py:clear_discovered_repos */
static json_t *pdb_clear_discovered_repos(pdb_io_t *io, const json_t *policy_key) {
    /* with write_txn(conn): _clear_repos_locked(conn, True, policy_key) */
    pdb_cr_ctx_t ctx = { true, policy_key };
    json_t *r = pdb_write_txn(io, pdb_cr_body, &ctx);
    if (pdb_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: pdb_owns @ hermes_cli/projects_db.py:owns */
static json_t *pdb_owns(pdb_io_t *io, const char *folder, const char *target) {
    (void)io;
    /* stem = folder.rstrip("/\\")
     * return target == folder or target.startswith(stem + os.sep)
     *        or target.startswith(stem + "/")   (os double: sep == "/") */
    char *stem = pdb_rstrip_chars(folder ? folder : "", "/\\");
    char *p1 = pdb_asprintf("%s/", stem ? stem : "");
    bool r = false;
    if (strcmp(target ? target : "", folder ? folder : "") == 0) r = true;
    if (!r && strncmp(target ? target : "", p1, strlen(p1)) == 0) r = true;
    if (!r && strncmp(target ? target : "", p1, strlen(p1)) == 0) r = true;
    free(p1);
    free(stem);
    return json_bool(r);
}

/* ── dispatch ─────────────────────────────────────────────────────────────── */
static json_t *pdb_dispatch(const char *func, const json_t *in, pdb_io_t *io) {
    if (strcmp(func, "projects_db_path") == 0)
        return pdb_projects_db_path(io);
    if (strcmp(func, "_slugify") == 0)
        return pdb_slugify(io, json_obj_get(in, "name"));
    if (strcmp(func, "_now") == 0)
        return pdb_now(io);
    if (strcmp(func, "connect") == 0)
        return pdb_connect(io, json_obj_get(in, "db_path"));
    if (strcmp(func, "_initialize") == 0)
        return pdb_initialize(io, json_obj_get(in, "conn"), json_get_str(in, "resolved", ""));
    if (strcmp(func, "connect_closing") == 0)
        return pdb_connect_closing(io, json_obj_get(in, "db_path"), json_obj_get(in, "body"));
    if (strcmp(func, "ProjectFolder.to_dict") == 0)
        return pdb_folder_to_dict(io, json_obj_get(in, "folder"), "ProjectFolder");
    if (strcmp(func, "Project.to_dict") == 0)
        return pdb_project_to_dict(io, json_obj_get(in, "project"));
    if (strcmp(func, "_load_project") == 0)
        return pdb_load_project(io, json_obj_get(in, "row"));
    if (strcmp(func, "_primary_path_key") == 0)
        return pdb_primary_path_key(io, json_obj_get(in, "path"));
    if (strcmp(func, "find_by_primary_path") == 0)
        return pdb_find_by_primary_path(io, json_obj_get(in, "path"),
                                        json_get_bool(in, "include_archived", false));
    if (strcmp(func, "_execute_rowcount") == 0)
        return pdb_execute_rowcount(io, json_obj_get(in, "sql"), json_obj_get(in, "params"));
    if (strcmp(func, "_upsert_meta_locked") == 0)
        return pdb_upsert_meta_locked(io, json_obj_get(in, "key"), json_obj_get(in, "value"));
    if (strcmp(func, "_get_meta") == 0)
        return pdb_get_meta(io, json_obj_get(in, "key"));
    if (strcmp(func, "_clear_repos_locked") == 0)
        return pdb_clear_repos_locked(io, json_get_bool(in, "clear", false),
                                      json_obj_get(in, "policy_key"));
    if (strcmp(func, "get_discovery_policy_key") == 0)
        return pdb_get_discovery_policy_key(io);
    if (strcmp(func, "reconcile_discovered_repos_policy") == 0)
        return pdb_reconcile_discovered_repos_policy(io, json_obj_get(in, "policy_key"),
                                                     json_get_bool(in, "preserve_unversioned", false));
    if (strcmp(func, "clear_discovered_repos") == 0)
        return pdb_clear_discovered_repos(io, json_obj_get(in, "policy_key"));
    if (strcmp(func, "owns") == 0)
        return pdb_owns(io, json_get_str(in, "folder", ""), json_get_str(in, "target", ""));
    return pdb_raise("ValueError", "unknown func");
}