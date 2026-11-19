/*
 * skills_tool_flow.c — FLOW port of the UNCLAIMED remainder of
 * tools/skills_tool.py (the skills tool surface: list/view progressive
 * disclosure). The CLAIMED siblings (skills_list / skill_view /
 * find_all_skills / sort_skills / _skills_dir / _skills_scan_signature /
 * _get_category_from_path / _parse_tags / _is_skill_disabled / load_env /
 * _get_disabled_skill_names / _parse_frontmatter / skill_matches_platform /
 * skill_matches_environment / the setup + plugin helpers) are SEAMED here as
 * universal-seam calls; every foreign-module call (agent.skill_utils,
 * hermes_cli.plugins, plugins.memory, tools.terminal_scope / env_passthrough /
 * credential_files / hosted_tool_policy, the clock, the filesystem and the
 * logger) rides the SAME seam. Ported targets call each other REAL (internal
 * composition); pure Path algebra (parts / join / parent / name / stem /
 * suffix / with_suffix / relative_to / is_relative_to / as_posix) is
 * transcribed inline as un-annotated helpers (no PoP re-claim).
 *
 * Ports the DECISION SEQUENCES of (18 UNCLAIMED defs):
 *   cache/signature — clear_skills_cache, _dir_tree_signature
 *   secret capture  — bind_turn_secret_capture, _skill_utils_delegate
 *   catalog         — skill_search_dirs, _skill_catalog
 *   hosting         — _host_skills
 *   plugin resolve  — _legacy_portable_skill_name, _resolve_plugin_skill
 *   path predicates — _under_any, _is_package_owned_markdown,
 *                     _owning_search_dir, _owned_relative
 *   candidates      — _collect_skill_candidates, _skill_linked_files
 *   readiness       — _skill_readiness
 *   locate          — locate_skill, _log_security_warnings
 *
 * Modeling contract (shared with tests/oracle_skills_tool_flow.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    name identifies the live call; args is a BORROWED JSON object the stub
 *    copies into the trace; the result is a NEW json_t (NULL == None;
 *    {"__raise__":kind,"message":m} == a raised exception). The port OWNS args
 *    (skt_call frees them) and the result.
 *  - Paths travel as POSIX strings. The oracle materializes them to
 *    PurePosixPath (ns["Path"] = PurePosixPath so str() keeps '/' on Windows)
 *    and monkeypatches the FS-touching methods (exists / is_dir / is_file /
 *    resolve / rglob / glob) to fire seams; the pure Path algebra runs REAL on
 *    both sides. DOMAIN: paths are non-empty POSIX strings; with_suffix /
 *    relative_to error messages name the PurePosixPath class (the oracle's
 *    Path double). Integral floats read as ints; fixture numbers in str()
 *    positions are integral-int or non-integral (ASCII scope).
 *  - Seam names -> live calls (args key sets are EXACT, nulls kept):
 *      _skills_dir {} -> path str                       (claimed sibling)
 *      get_skill_search_roots {local} -> [[tier,dir]]   (skill_utils)
 *      path.exists / path.is_dir / path.is_file {path} -> bool
 *      path.resolve {path} -> path str (may raise)
 *      path.rglob / path.glob {path,pattern} -> [path]
 *      _get_disabled_skill_names {} -> [names] (live set; sorted internally)
 *      _skills_scan_signature {dirs,disabled} -> [sig,disabled,platform]
 *      monotonic {} -> number                           (time.monotonic)
 *      iter_project_skill_files {dir} -> [path]
 *      iter_skill_index_files {dir,filename} -> [path]
 *      _read_skill_text {path} -> str                   (plugin)
 *      _parse_frontmatter {content} -> [frontmatter,body]
 *      _truncate_description {description} -> str       (plugin)
 *      _get_category_from_path {path} -> str|null       (claimed sibling)
 *      skill_matches_platform / skill_matches_environment /
 *      skill_matches_apps {frontmatter} -> bool
 *      resolve_skill_catalog {entries} -> [row]         (skill_utils)
 *      is_disabled_entry {entry,disabled} -> bool       (skill_utils)
 *      _safe_frontmatter {path,content} -> dict         (plugin)
 *      _is_skill_support_path {path} -> bool            (skill_utils alias)
 *      parse_qualified_name {name} -> [namespace,bare]
 *      is_valid_namespace {candidate} -> bool
 *      discover_plugins {} -> null                      (hermes_cli.plugins)
 *      get_plugin_manager {} -> pm dict
 *      pm.find_plugin_skill {name} -> path|null
 *      pm.remove_plugin_skill {name} -> null
 *      pm.list_plugin_skills {namespace} -> [name]
 *      _get_active_memory_provider {} -> str|null       (plugins.memory)
 *      _prune_inactive_memory_provider_skills {provider} -> null
 *      load_memory_provider {namespace} -> null
 *      _serve_plugin_skill {skill_md,namespace,bare,file_path,preprocess,
 *                           session_id} -> str          (claimed plugin)
 *      _fail {error,extra} -> str                       (plugin)
 *      host_feature_refusal {feature,fn_name} -> str|null
 *      fn_call {name,args,kwargs} -> value              (wrapped fn double)
 *      terminal_env {key,default} -> value              (terminal_scope)
 *      load_env {} -> dict                              (claimed sibling)
 *      _is_env_var_persisted {name,snapshot} -> bool    (claimed setup)
 *      _get_required_environment_variables {frontmatter} -> [var]
 *      _capture_required_environment_variables {skill_name,missing} -> dict
 *      register_env_passthrough {names} -> [refused]    (env_passthrough)
 *      register_credential_files {files} -> [missing]   (credential_files)
 *      _build_setup_note {status,missing,setup_help} -> str|null (claimed)
 *      _is_remote_env_backend {backend} -> bool         (claimed setup)
 *      pick_skill_candidate {candidates} -> [won,contenders]
 *      skill_candidate_rank {skill_md,root} -> [bool,int]
 *      is_quarantined_project_skill {skill_md} -> bool
 *      sort_skills {skills} -> [row]                    (claimed sibling)
 *      find_all_skills {skip_disabled} -> [row]         (claimed sibling)
 *      os.stat {path} -> {mtime_ns}                     (_dir_tree_signature)
 *      os.scandir {path} -> [{name}]
 *      entry.is_dir {path,name} -> bool
 *      turn_capture.set {callback} -> token             (ContextVar double)
 *      turn_capture.reset {token} -> null
 *      cm_body {} -> value                              (with-block body)
 *      skill_utils_call {attr,args} -> value            (_delegate target)
 *      logger.debug / logger.info / logger.warning {msg,args,exc_info}
 *  - State mirror (in["state"], copied by the harness before mutation):
 *      {"skills_cache": {key: [signature, timestamp, rows]}}
 *    key = (skip_disabled?"with_disabled":"filtered") + "|" +
 *          (include_hidden?"true":"false"). Only clear_skills_cache and
 *    _skill_catalog touch it; seamed siblings abstract their effects.
 *  - pm double: {"_plugins": {key: {"manifest": {"portable","data_namespace"}}},
 *    "_plugin_skills": {qualified: {"plugin_key","bare_name"}}}. The oracle
 *    materializes attribute doubles; C reads the JSON fields directly.
 *  - Exception kinds: the FULL builtin hierarchy is transcribed (skt_exc_parent)
 *    so `except Exception` / suppress(Exception) swallow every Exception family
 *    member while the BaseException trio (SystemExit / KeyboardInterrupt /
 *    GeneratorExit) passes through. suppress(OSError) / (UnicodeDecodeError,
 *    PermissionError) catch their families exactly.
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - Self-contained: the harness #includes this .c; every symbol is static and
 *    skt_*-prefixed; only libjson is linked.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

/* ── live module constants (byte-exact) ───────────────────────────────────── */
#define SKT_MAX_NAME_LENGTH 64
#define SKT_CACHE_TTL 30.0
#define SKT_SIGNATURE_MAX_DEPTH 6
#define SKT_TIER_PROJECT 0
#define SKT_TIER_LOCAL 1
static const char SKT_AMBIGUOUS_PREFIX[] = "Ambiguous skill name ";
static const char *SKT_EXCLUDED_DIRS[] = {
    ".git", ".github", ".hub", ".archive", ".curator_backups", ".locks",
    ".hermes-dist", ".venv", "venv", "node_modules", "site-packages",
    "__pycache__", ".tox", ".nox", ".pytest_cache", ".mypy_cache", ".ruff_cache",
};
static const char *SKT_SUPPORT_DIRS[] = {"references", "templates", "assets", "scripts"};
static const char *SKT_INJECTION_PATTERNS[] = {
    "ignore previous instructions", "ignore all previous", "you are now",
    "disregard your", "forget your instructions", "new instructions:",
    "system prompt:", "<system>", "]]>",
};
typedef struct { const char *sub; const char *globs[8]; size_t ng; bool recursive, files_only; } skt_link_spec;
static const skt_link_spec SKT_LINK_SPECS[] = {
    {"references", {"*.md"}, 1, false, false},
    {"templates", {"*.md", "*.py", "*.yaml", "*.yml", "*.json", "*.tex", "*.sh"}, 7, true, false},
    {"assets", {"*"}, 1, true, true},
    {"scripts", {"*.py", "*.sh", "*.bash", "*.js", "*.ts", "*.rb"}, 6, false, false},
};

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct skt_env {
    json_t *state;
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
} skt_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool skt_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool skt_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool skt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool skt_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool skt_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *skt_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *skt_sval(const json_t *v) { const char *s = skt_str(v); return s ? s : ""; }
static bool skt_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const char *skt_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── raise specs ──────────────────────────────────────────────────────────── */
static bool skt_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && skt_is_str(json_obj_get(r, "__raise__"));
}
static const char *skt_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static json_t *skt_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *skt_attr_raise(const json_t *v, const char *attr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", skt_type_name(v), attr);
    return skt_raise_spec("AttributeError", buf);
}
static json_t *skt_subscript_raise(const json_t *v) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", skt_type_name(v));
    return skt_raise_spec("TypeError", buf);
}
static json_t *skt_keyerror_raise(const char *key) {
    char buf[256];
    snprintf(buf, sizeof(buf), "'%s'", key);
    return skt_raise_spec("KeyError", buf);
}
static json_t *skt_valueerror_raise(const char *msg) { return skt_raise_spec("ValueError", msg); }

/* ── FULL builtin exception hierarchy ─────────────────────────────────────── */
static const char *skt_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"LookupError", "Exception"}, {"ArithmeticError", "Exception"},
        {"OSError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
        {"ImportError", "Exception"}, {"RuntimeError", "Exception"},
        {"Warning", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"ExceptionGroup", "Exception"},
        {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"RecursionError", "RuntimeError"}, {"NotImplementedError", "RuntimeError"},
        {"UnboundLocalError", "NameError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"FloatingPointError", "ArithmeticError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ModuleNotFoundError", "ImportError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].c, c) == 0) return T[i].p;
    return NULL;
}
static bool skt_exc_is(const char *kind, const char *target) {
    const char *cls = kind;
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cls, target) == 0) return true;
        const char *p = skt_exc_parent(cls);
        if (!p) {
            if (strcmp(cls, "BaseException") == 0) return false;
            p = "Exception";   /* unknown -> synthetic Exception subclass */
        }
        cls = p;
    }
    return false;
}
static bool skt_exc_is_exception(const char *kind) { return skt_exc_is(kind, "Exception"); }
/* except Exception: swallow plain Exception (free r); return true to propagate. */
static bool skt_swallow_exc(json_t *r) {
    if (skt_exc_is_exception(skt_kind(r))) { json_free(r); return false; }
    return true;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *skt_call(skt_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *skt_seam0(skt_env *e, const char *name) { return skt_call(e, name, json_object()); }

/* ── deep JSON equality (order-independent objects, ordered arrays) ───────── */
static bool skt_json_eq(const json_t *a, const json_t *b) {
    if (skt_is_null(a) && skt_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(skt_sval(a), skt_sval(b)) == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!skt_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !skt_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/* ── string buffer ────────────────────────────────────────────────────────── */
typedef struct { char *s; size_t len, cap; } skt_buf;
static void skt_buf_init(skt_buf *b) { b->cap = 64; b->len = 0; b->s = (char *)malloc(b->cap); b->s[0] = '\0'; }
static void skt_buf_add(skt_buf *b, const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->s = (char *)realloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static char *skt_buf_take(skt_buf *b) { char *r = b->s; b->s = NULL; return r; }
static char *skt_strdup(const char *s) { return strdup(s ? s : ""); }
static char *skt_concat3(const char *a, const char *b, const char *c) {
    skt_buf buf; skt_buf_init(&buf); skt_buf_add(&buf, a); skt_buf_add(&buf, b); skt_buf_add(&buf, c);
    return skt_buf_take(&buf);
}
static char *skt_join_strs(const json_t *arr, const char *sep) {
    skt_buf buf; skt_buf_init(&buf);
    for (size_t i = 0; i < json_len(arr); i++) {
        if (i) skt_buf_add(&buf, sep);
        skt_buf_add(&buf, skt_sval(json_get(arr, i)));
    }
    return skt_buf_take(&buf);
}

/* ── str() / repr() / strip / lower (CPython 3.14, ASCII domain) ──────────── */
static char *skt_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return skt_strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return skt_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return skt_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            double d = v->num_val;
            char buf[64];
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return skt_strdup(buf);
        }
        default: return skt_strdup("");
    }
}
static char *skt_pyrepr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *r = (char *)malloc(cap);
    size_t o = 0;
    r[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { r[o++] = '\\'; r[o++] = (char)c; }
        else if (c == '\n') { r[o++] = '\\'; r[o++] = 'n'; }
        else if (c == '\r') { r[o++] = '\\'; r[o++] = 'r'; }
        else if (c == '\t') { r[o++] = '\\'; r[o++] = 't'; }
        else if (c < 0x20 || c == 0x7F) { o += (size_t)snprintf(r + o, cap - o, "\\x%02x", c); }
        else r[o++] = (char)c;
    }
    r[o++] = q; r[o] = '\0';
    return r;
}
static bool skt_ascii_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *skt_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && skt_ascii_ws(s[a])) a++;
    while (b > a && skt_ascii_ws(s[b - 1])) b--;
    char *r = (char *)malloc(b - a + 1);
    memcpy(r, s + a, b - a); r[b - a] = '\0';
    return r;
}
static char *skt_lower(const char *s) {
    char *r = skt_strdup(s);
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
static char *skt_upper(const char *s) {
    char *r = skt_strdup(s);
    for (char *p = r; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    return r;
}
static char *skt_slice(const char *s, size_t n) {
    size_t l = strlen(s); if (n < l) l = n;
    char *r = (char *)malloc(l + 1); memcpy(r, s, l); r[l] = '\0';
    return r;
}

/* ── PurePosixPath algebra (inline, pure) ─────────────────────────────────── */
typedef struct { char **v; size_t n; bool abs; } skt_parts;
static void skt_parts_free(skt_parts *p) {
    for (size_t i = 0; i < p->n; i++) free(p->v[i]);
    free(p->v); p->v = NULL; p->n = 0;
}
static skt_parts skt_parts_parse(const char *s) {
    skt_parts p; p.v = NULL; p.n = 0; p.abs = (s && s[0] == '/');
    size_t cap = 0;
    const char *cur = s ? s : "";
    while (*cur) {
        while (*cur == '/') cur++;
        if (!*cur) break;
        const char *start = cur;
        while (*cur && *cur != '/') cur++;
        size_t len = (size_t)(cur - start);
        if (len == 1 && start[0] == '.') continue;
        if (p.n == cap) { cap = cap ? cap * 2 : 8; p.v = (char **)realloc(p.v, cap * sizeof(char *)); }
        char *c = (char *)malloc(len + 1); memcpy(c, start, len); c[len] = '\0';
        p.v[p.n++] = c;
    }
    return p;
}
static char *skt_parts_str(const skt_parts *p) {
    skt_buf b; skt_buf_init(&b);
    if (p->abs) skt_buf_add(&b, "/");
    for (size_t i = 0; i < p->n; i++) { if (i) skt_buf_add(&b, "/"); skt_buf_add(&b, p->v[i]); }
    if (!p->abs && p->n == 0) { free(b.s); return skt_strdup("."); }
    return skt_buf_take(&b);
}
static char *skt_path_norm(const char *s) { skt_parts p = skt_parts_parse(s); char *r = skt_parts_str(&p); skt_parts_free(&p); return r; }
static char *skt_path_join(const char *a, const char *b) {
    skt_parts pb = skt_parts_parse(b);
    if (pb.abs) { char *r = skt_parts_str(&pb); skt_parts_free(&pb); return r; }
    skt_parts pa = skt_parts_parse(a);
    skt_parts m; m.abs = pa.abs; m.n = pa.n + pb.n;
    m.v = (char **)malloc((m.n ? m.n : 1) * sizeof(char *));
    for (size_t i = 0; i < pa.n; i++) m.v[i] = skt_strdup(pa.v[i]);
    for (size_t i = 0; i < pb.n; i++) m.v[pa.n + i] = skt_strdup(pb.v[i]);
    char *r = skt_parts_str(&m);
    skt_parts_free(&pa); skt_parts_free(&pb); skt_parts_free(&m);
    return r;
}
static char *skt_path_name(const char *s) {
    skt_parts p = skt_parts_parse(s);
    char *r = skt_strdup(p.n ? p.v[p.n - 1] : "");
    skt_parts_free(&p);
    return r;
}
static char *skt_path_parent(const char *s) {
    skt_parts p = skt_parts_parse(s);
    skt_parts q; q.abs = p.abs; q.n = p.n ? p.n - 1 : 0; q.v = p.v;
    char *r = skt_parts_str(&q);
    q.v = NULL; q.n = 0;
    skt_parts_free(&p);
    return r;
}
static void skt_name_stem_suffix(const char *name, char **stem, char **suffix) {
    const char *dot = NULL;
    for (const char *p = name + 1; *p; p++) if (*p == '.') dot = p;
    if (dot) {
        size_t sl = (size_t)(dot - name);
        *stem = (char *)malloc(sl + 1); memcpy(*stem, name, sl); (*stem)[sl] = '\0';
        *suffix = skt_strdup(dot);
    } else { *stem = skt_strdup(name); *suffix = skt_strdup(""); }
}
static char *skt_with_suffix(const char *s, const char *suf, json_t **err) {
    *err = NULL;
    char *norm = skt_path_norm(s);
    char *name = skt_path_name(s);
    if (!name[0]) {
        char buf[512];
        snprintf(buf, sizeof(buf), "PurePosixPath('%s') has an empty name", norm);
        *err = skt_valueerror_raise(buf);
        free(norm); free(name); return NULL;
    }
    if (suf[0] && suf[0] != '.') {
        char *rep = skt_pyrepr(suf);
        char *msg = skt_concat3("Invalid suffix ", rep, "");
        *err = skt_valueerror_raise(msg);
        free(rep); free(msg); free(norm); free(name); return NULL;
    }
    char *stem, *oldsuf;
    skt_name_stem_suffix(name, &stem, &oldsuf);
    char *newname = skt_concat3(stem, suf, "");
    char *parent = skt_path_parent(s);
    char *r = skt_path_join(parent, newname);
    free(stem); free(oldsuf); free(newname); free(parent); free(norm); free(name);
    return r;
}
static bool skt_is_relative_to(const char *s, const char *root) {
    skt_parts p = skt_parts_parse(s), r = skt_parts_parse(root);
    bool ok;
    if (r.n == 0) ok = (p.n == 0);
    else if (p.n < r.n) ok = false;
    else {
        ok = true;
        for (size_t i = 0; i < r.n; i++) if (strcmp(p.v[i], r.v[i]) != 0) { ok = false; break; }
    }
    skt_parts_free(&p); skt_parts_free(&r);
    return ok;
}
static char *skt_relative_to(const char *s, const char *root, json_t **err) {
    *err = NULL;
    if (!skt_is_relative_to(s, root)) {
        char *ns = skt_path_norm(s), *nr = skt_path_norm(root);
        skt_buf b; skt_buf_init(&b);
        skt_buf_add(&b, "'"); skt_buf_add(&b, ns); skt_buf_add(&b, "' is not in the subpath of '");
        skt_buf_add(&b, nr); skt_buf_add(&b, "'");
        *err = skt_valueerror_raise(b.s);
        free(ns); free(nr); free(b.s);
        return NULL;
    }
    skt_parts p = skt_parts_parse(s), r = skt_parts_parse(root);
    skt_parts q; q.abs = false; q.n = p.n - r.n;
    q.v = (char **)malloc((q.n ? q.n : 1) * sizeof(char *));
    for (size_t i = 0; i < q.n; i++) q.v[i] = skt_strdup(p.v[r.n + i]);
    char *out = skt_parts_str(&q);
    skt_parts_free(&p); skt_parts_free(&r); skt_parts_free(&q);
    return out;
}
static size_t skt_parts_count(const char *s) {
    skt_parts p = skt_parts_parse(s);
    size_t n = p.n + (p.abs ? 1 : 0);
    skt_parts_free(&p);
    return n;
}
static bool skt_in_strset(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return true;
    return false;
}

/* ── sorted string-set helpers ────────────────────────────────────────────── */
static void skt_sort_strs(json_t *arr) {
    for (size_t i = 1; i < json_len(arr); i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(skt_sval(arr->c.items[j - 1]), skt_sval(key)) > 0) {
            arr->c.items[j] = arr->c.items[j - 1]; j--;
        }
        arr->c.items[j] = key;
    }
}
static bool skt_arr_has_str(const json_t *arr, const char *s) {
    for (size_t i = 0; i < json_len(arr); i++) if (strcmp(skt_sval(json_get(arr, i)), s) == 0) return true;
    return false;
}
static void skt_set_add(json_t *arr, const char *s) {
    if (skt_arr_has_str(arr, s)) return;
    json_append(arr, json_string(s));
    skt_sort_strs(arr);
}
static json_t *skt_norm_set(const json_t *v) {
    json_t *out = json_array();
    if (skt_is_arr(v)) for (size_t i = 0; i < json_len(v); i++) skt_set_add(out, skt_sval(json_get(v, i)));
    return out;
}

/* ── logger seam ──────────────────────────────────────────────────────────── */
static json_t *skt_log(skt_env *e, const char *level, const char *msg, json_t *args, bool exc_info) {
    char name[32]; snprintf(name, sizeof(name), "logger.%s", level);
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args ? args : json_array());
    json_set(a, "exc_info", json_bool(exc_info));
    return skt_call(e, name, a);
}
static json_t *skt_log_args1(const char *s) {
    json_t *a = json_array(); json_append(a, json_string(s ? s : "")); return a;
}

/* ── state cache access ───────────────────────────────────────────────────── */
static json_t *skt_cache(skt_env *e) {
    json_t *c = json_obj_get(e->state, "skills_cache");
    if (!skt_is_obj(c)) { c = json_object(); json_set(e->state, "skills_cache", c); }
    return c;
}
static char *skt_cache_key(bool skip_disabled, bool include_hidden) {
    return skt_concat3(skip_disabled ? "with_disabled" : "filtered", "|", include_hidden ? "true" : "false");
}

/* ── path FS seams ────────────────────────────────────────────────────────── */
static json_t *skt_path_bool(skt_env *e, const char *op, const char *path) {
    json_t *a = json_object(); json_set(a, "path", json_string(path));
    char name[32]; snprintf(name, sizeof(name), "path.%s", op);
    return skt_call(e, name, a);
}
static json_t *skt_path_resolve(skt_env *e, const char *path) {
    json_t *a = json_object(); json_set(a, "path", json_string(path));
    return skt_call(e, "path.resolve", a);
}
static json_t *skt_path_glob(skt_env *e, const char *op, const char *path, const char *pattern) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path));
    json_set(a, "pattern", json_string(pattern));
    char name[32]; snprintf(name, sizeof(name), "path.%s", op);
    return skt_call(e, name, a);
}

/* ── forward declarations (internal composition) ──────────────────────────── */
static json_t *skt_dts_rec(skt_env *, const char *, json_t *, double);
static json_t *skt_skill_search_dirs(skt_env *, const json_t *);
static json_t *skt_collect_skill_candidates(skt_env *, const json_t *);
static json_t *skt_owning_search_dir(skt_env *, const json_t *);
static json_t *skt_owned_relative(skt_env *, const json_t *);
static json_t *skt_is_package_owned_markdown(skt_env *, const char *, const char *);
static json_t *skt_under_any(skt_env *, const char *, const json_t *);
static json_t *skt_legacy_portable_skill_name(skt_env *, const json_t *, const char *, const char *);

/* ══════════════════════════════════════════════════════════════════════════ */
/* PoP: skt_clear_skills_cache @ tools/skills_tool.py:clear_skills_cache */
static json_t *skt_clear_skills_cache(skt_env *e, const json_t *in) {
    (void)in;
    json_t *c = skt_cache(e);
    while (c->c.count) json_obj_del(c, c->c.keys[0]);
    return NULL;
}

/* PoP: skt_dir_tree_signature @ tools/skills_tool.py:_dir_tree_signature */
static json_t *skt_dir_tree_signature(skt_env *e, const json_t *in) {
    const char *root = json_get_str(in, "root", "");
    double depth = json_get_num(in, "depth", SKT_SIGNATURE_MAX_DEPTH);
    json_t *sig = json_array();   /* out-param model: returned as ret */
    json_t *err = skt_dts_rec(e, root, sig, depth);
    if (err) { json_free(sig); return err; }
    return sig;
}
static json_t *skt_dts_rec(skt_env *e, const char *root, json_t *sig, double depth) {
    json_t *sa = json_object(); json_set(sa, "path", json_string(root));
    json_t *st = skt_call(e, "os.stat", sa);
    if (skt_is_raise(st)) {
        if (skt_exc_is(skt_kind(st), "OSError")) { json_free(st); return NULL; }
        return st;
    }
    json_t *mt = json_obj_get(st, "mtime_ns");
    double mtime = skt_is_num(mt) ? mt->num_val : 0;
    json_t *da = json_object(); json_set(da, "path", json_string(root));
    json_t *entries = skt_call(e, "os.scandir", da);
    if (skt_is_raise(entries)) {
        json_free(st);
        if (skt_exc_is(skt_kind(entries), "OSError")) { json_free(entries); return NULL; }
        return entries;
    }
    json_t *names = json_array();
    bool is_skill = false;
    json_t *prop = NULL;
    if (skt_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *ent = json_get(entries, i);
            const char *nm = json_get_str(ent, "name", "");
            json_t *ia = json_object();
            json_set(ia, "path", json_string(root));
            json_set(ia, "name", json_string(nm));
            json_t *isd = skt_call(e, "entry.is_dir", ia);
            if (skt_is_raise(isd)) {
                if (skt_exc_is(skt_kind(isd), "OSError")) { json_free(isd); continue; }
                prop = isd; break;
            }
            bool d = skt_truth(isd);
            json_free(isd);
            if (d) json_append(names, json_string(nm));
            else if (strcmp(nm, "SKILL.md") == 0) is_skill = true;
        }
    }
    json_free(entries);
    if (prop) { json_free(names); json_free(st); return prop; }
    skt_sort_strs(names);
    json_t *row = json_array();
    json_append(row, json_string(root));
    json_append(row, json_number(mtime));
    json_append(row, names);
    json_append(sig, row);
    json_free(st);
    if (depth <= 0) return NULL;
    for (size_t i = 0; i < json_len(names); i++) {
        const char *nm = skt_sval(json_get(names, i));
        if (skt_in_strset(nm, SKT_EXCLUDED_DIRS, sizeof(SKT_EXCLUDED_DIRS) / sizeof(SKT_EXCLUDED_DIRS[0])))
            continue;
        if (is_skill && skt_in_strset(nm, SKT_SUPPORT_DIRS, 4)) continue;
        char *child = skt_path_join(root, nm);
        json_t *err = skt_dts_rec(e, child, sig, depth - 1);
        free(child);
        if (err) return err;
    }
    return NULL;
}

/* PoP: skt_bind_turn_secret_capture @ tools/skills_tool.py:bind_turn_secret_capture */
static json_t *skt_bind_turn_secret_capture(skt_env *e, const json_t *in) {
    json_t *cb = json_obj_get(in, "callback");
    json_t *sa = json_object();
    json_set(sa, "callback", cb ? json_copy(cb) : json_null());
    json_t *token = skt_call(e, "turn_capture.set", sa);
    if (skt_is_raise(token)) return token;
    json_t *body = skt_seam0(e, "cm_body");
    json_t *ra = json_object();
    json_set(ra, "token", token ? json_copy(token) : json_null());
    json_t *reset = skt_call(e, "turn_capture.reset", ra);
    json_free(token);
    if (skt_is_raise(reset)) { json_free(body); return reset; }
    json_free(reset);
    if (skt_is_raise(body)) return body;
    json_free(body);
    return NULL;
}

/* PoP: skt_skill_utils_delegate @ tools/skills_tool.py:_skill_utils_delegate */
static json_t *skt_skill_utils_delegate(skt_env *e, const json_t *in) {
    const char *attr = json_get_str(in, "attr", "");
    json_t *args = json_obj_get(in, "args");
    json_t *ca = json_object();
    json_set(ca, "attr", json_string(attr));
    json_set(ca, "args", args ? json_copy(args) : json_array());
    json_t *r = skt_call(e, "skill_utils_call", ca);
    if (skt_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "attr", json_string(attr));
    json_set(out, "name", json_string(attr));
    json_set(out, "qualname", json_string(attr));
    json_set(out, "result", r ? r : json_null());
    return out;
}

/* PoP: skt_skill_search_dirs @ tools/skills_tool.py:skill_search_dirs */
static json_t *skt_skill_search_dirs(skt_env *e, const json_t *in) {
    (void)in;
    json_t *active = skt_seam0(e, "_skills_dir");
    if (skt_is_raise(active)) return active;
    char *active_s = skt_pystr(active);
    json_t *ga = json_object(); json_set(ga, "local", json_string(active_s));
    json_t *roots_raw = skt_call(e, "get_skill_search_roots", ga);
    if (skt_is_raise(roots_raw)) { free(active_s); json_free(active); return roots_raw; }
    json_t *roots = json_array();
    json_t *prop = NULL;
    if (skt_is_arr(roots_raw)) {
        for (size_t i = 0; i < json_len(roots_raw); i++) {
            json_t *pair = json_get(roots_raw, i);
            json_t *tnode = json_get(pair, 0);
            double tier = skt_is_num(tnode) ? tnode->num_val : 0;
            const char *dir = skt_sval(json_get(pair, 1));
            bool keep = true;
            if ((int)tier == SKT_TIER_LOCAL) {
                json_t *ex = skt_path_bool(e, "exists", dir);
                if (skt_is_raise(ex)) { prop = ex; break; }
                keep = skt_truth(ex);
                json_free(ex);
            }
            if (keep) {
                json_t *p = json_array();
                json_append(p, json_number(tier));
                json_append(p, json_string(dir));
                json_append(roots, p);
            }
        }
    }
    json_free(roots_raw);
    if (prop) { json_free(roots); free(active_s); json_free(active); return prop; }
    json_t *out = json_array();
    json_append(out, roots);
    json_append(out, json_string(active_s));
    free(active_s); json_free(active);
    return out;
}

/* per-file scan body of _skill_catalog: returns a row object OR a raise spec. */
static json_t *skt_catalog_one(skt_env *e, const char *skill_md, double tier, const char *scan_dir) {
    json_t *err = NULL, *row = NULL, *fm = NULL, *body = NULL, *desc = NULL;
    json_t *trunc = NULL, *cat = NULL, *text = NULL, *pf = NULL;
    char *name_s = NULL;
    json_t *ra = json_object(); json_set(ra, "path", json_string(skill_md));
    text = skt_call(e, "_read_skill_text", ra);
    if (skt_is_raise(text)) { err = text; text = NULL; goto done; }
    if (skt_is_str(text)) {
        char *content = skt_slice(skt_str(text), 4000);
        json_t *pa = json_object(); json_set(pa, "content", json_string(content)); free(content);
        pf = skt_call(e, "_parse_frontmatter", pa);
    } else if (skt_is_arr(text)) {
        json_t *sl = json_array();
        for (size_t k = 0; k < json_len(text) && k < 4000; k++) json_append(sl, json_copy(json_get(text, k)));
        json_t *pa = json_object(); json_set(pa, "content", sl);
        pf = skt_call(e, "_parse_frontmatter", pa);
    } else if (skt_is_obj(text)) {
        err = skt_raise_spec("KeyError", "slice(None, 4000, None)"); goto done;
    } else {
        err = skt_subscript_raise(text); goto done;
    }
    if (skt_is_raise(pf)) { err = pf; pf = NULL; goto done; }
    if (!skt_is_arr(pf)) { err = skt_raise_spec("TypeError", "cannot unpack non-iterable object"); goto done; }
    if (json_len(pf) != 2) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s values to unpack (expected 2, got %zu)",
                 json_len(pf) < 2 ? "not enough" : "too many", json_len(pf));
        err = skt_valueerror_raise(buf); goto done;
    }
    fm = json_copy(json_get(pf, 0)); body = json_copy(json_get(pf, 1));
    if (!skt_is_obj(fm)) { err = skt_attr_raise(fm, "get"); goto done; }
    desc = json_copy(json_obj_get(fm, "description")); if (!desc) desc = json_string("");
    if (!skt_truth(desc)) {
        if (!skt_is_str(body)) { err = skt_attr_raise(body, "strip"); goto done; }
        char *bs = skt_strip(skt_str(body));
        char *found = NULL, *p = bs;
        while (p && *p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            char *ln = skt_strip(p);
            if (ln[0] && ln[0] != '#') { found = ln; break; }
            free(ln);
            if (!nl) break;
            p = nl + 1;
        }
        free(bs);
        if (found) { json_free(desc); desc = json_string(found); free(found); }
    }
    {
        json_t *namev = json_obj_get(fm, "name");
        if (!namev) { char *parent = skt_path_parent(skill_md); name_s = skt_path_name(parent); free(parent); }
        else if (skt_is_str(namev)) name_s = skt_slice(skt_str(namev), SKT_MAX_NAME_LENGTH);
        else { err = skt_subscript_raise(namev); goto done; }
    }
    {
        json_t *ta = json_object(); json_set(ta, "description", json_copy(desc));
        trunc = skt_call(e, "_truncate_description", ta);
        if (skt_is_raise(trunc)) { err = trunc; trunc = NULL; goto done; }
    }
    {
        json_t *ca = json_object(); json_set(ca, "path", json_string(skill_md));
        cat = skt_call(e, "_get_category_from_path", ca);
        if (skt_is_raise(cat)) { err = cat; cat = NULL; goto done; }
    }
    {
        bool visible = false;
        json_t *fa = json_object(); json_set(fa, "frontmatter", json_copy(fm));
        json_t *m1 = skt_call(e, "skill_matches_platform", fa);
        if (skt_is_raise(m1)) { err = m1; goto done; }
        if (skt_truth(m1)) {
            json_t *fa2 = json_object(); json_set(fa2, "frontmatter", json_copy(fm));
            json_t *m2 = skt_call(e, "skill_matches_environment", fa2);
            if (skt_is_raise(m2)) { json_free(m1); err = m2; goto done; }
            if (skt_truth(m2)) {
                json_t *fa3 = json_object(); json_set(fa3, "frontmatter", json_copy(fm));
                json_t *m3 = skt_call(e, "skill_matches_apps", fa3);
                if (skt_is_raise(m3)) { json_free(m1); json_free(m2); err = m3; goto done; }
                visible = skt_truth(m3); json_free(m3);
            }
            json_free(m2);
        }
        json_free(m1);
        row = json_object();
        json_set(row, "name", json_string(name_s));
        json_set(row, "description", trunc); trunc = NULL;
        json_set(row, "category", cat); cat = NULL;
        json_set(row, "tier", json_number(tier));
        json_set(row, "root", json_string(scan_dir));
        json_set(row, "path", json_string(skill_md));
        json_set(row, "visible", json_bool(visible));
    }
done:
    json_free(text); json_free(pf); json_free(fm); json_free(body); json_free(desc);
    json_free(trunc); json_free(cat); free(name_s);
    if (err) { json_free(row); return err; }
    return row;
}

/* PoP: skt_skill_catalog @ tools/skills_tool.py:_skill_catalog */
static json_t *skt_skill_catalog(skt_env *e, const json_t *in) {
    bool skip_disabled = json_get_bool(in, "skip_disabled", false);
    bool include_hidden = json_get_bool(in, "include_hidden", false);
    char *ckey = skt_cache_key(skip_disabled, include_hidden);
    json_t *cache = skt_cache(e);
    json_t *disabled = json_array();
    if (!skip_disabled) {
        json_t *d = skt_seam0(e, "_get_disabled_skill_names");
        if (skt_is_raise(d)) { free(ckey); json_free(disabled); return d; }
        json_free(disabled);
        disabled = skt_norm_set(d);
        json_free(d);
    }
    json_t *ssd = skt_skill_search_dirs(e, in);
    if (skt_is_raise(ssd)) { json_free(disabled); free(ckey); return ssd; }
    json_t *roots = json_copy(json_get(ssd, 0));
    json_free(ssd);
    json_t *dirs = json_array();
    for (size_t i = 0; i < json_len(roots); i++) json_append(dirs, json_string(skt_sval(json_get(json_get(roots, i), 1))));
    json_t *sia = json_object();
    json_set(sia, "dirs", dirs);
    json_set(sia, "disabled", json_copy(disabled));
    json_t *signature = skt_call(e, "_skills_scan_signature", sia);
    if (skt_is_raise(signature)) { json_free(disabled); json_free(roots); free(ckey); return signature; }
    json_t *now = skt_seam0(e, "monotonic");
    if (skt_is_raise(now)) { json_free(signature); json_free(disabled); json_free(roots); free(ckey); return now; }
    double now_val = skt_is_num(now) ? now->num_val : 0;
    json_t *cached = json_obj_get(cache, ckey);
    if (skt_is_arr(cached) && json_len(cached) >= 3) {
        double cts_val = skt_is_num(json_get(cached, 1)) ? json_get(cached, 1)->num_val : 0;
        if (skt_json_eq(json_get(cached, 0), signature) && (now_val - cts_val) < SKT_CACHE_TTL) {
            json_t *out = json_array();
            json_t *crows = json_get(cached, 2);
            for (size_t i = 0; i < json_len(crows); i++) json_append(out, json_copy(json_get(crows, i)));
            json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey);
            return out;
        }
    }
    json_t *scanned = json_array();
    json_t *prop = NULL;
    for (size_t ri = 0; ri < json_len(roots) && !prop; ri++) {
        json_t *pair = json_get(roots, ri);
        double tier = skt_is_num(json_get(pair, 0)) ? json_get(pair, 0)->num_val : 0;
        const char *scan_dir = skt_sval(json_get(pair, 1));
        json_t *files;
        if ((int)tier == SKT_TIER_PROJECT) {
            json_t *a = json_object(); json_set(a, "dir", json_string(scan_dir));
            files = skt_call(e, "iter_project_skill_files", a);
        } else {
            json_t *a = json_object();
            json_set(a, "dir", json_string(scan_dir));
            json_set(a, "filename", json_string("SKILL.md"));
            files = skt_call(e, "iter_skill_index_files", a);
        }
        if (skt_is_raise(files)) { prop = files; break; }
        for (size_t fi = 0; fi < json_len(files); fi++) {
            const char *skill_md = skt_sval(json_get(files, fi));
            skt_parts pp = skt_parts_parse(skill_md);
            bool excluded = false;
            for (size_t k = 0; k < pp.n; k++)
                if (skt_in_strset(pp.v[k], SKT_EXCLUDED_DIRS, sizeof(SKT_EXCLUDED_DIRS) / sizeof(SKT_EXCLUDED_DIRS[0]))) { excluded = true; break; }
            skt_parts_free(&pp);
            if (excluded) continue;
            json_t *r = skt_catalog_one(e, skill_md, tier, scan_dir);
            if (!skt_is_raise(r)) { json_append(scanned, r); continue; }
            const char *k = skt_kind(r);
            char *es = skt_strdup(json_get_str(r, "message", ""));
            if (skt_exc_is(k, "UnicodeDecodeError") || skt_exc_is(k, "PermissionError")) {
                json_t *la = json_array();
                json_append(la, json_string(skill_md)); json_append(la, json_string(es));
                json_t *lr = skt_log(e, "debug", "Failed to read skill file %s: %s", la, false);
                if (skt_is_raise(lr)) { free(es); json_free(r); prop = lr; break; }
                json_free(lr);
            } else if (skt_exc_is_exception(k)) {
                json_t *la = json_array();
                json_append(la, json_string(skill_md)); json_append(la, json_string(es));
                json_t *lr = skt_log(e, "debug", "Skipping skill at %s: failed to parse: %s", la, true);
                if (skt_is_raise(lr)) { free(es); json_free(r); prop = lr; break; }
                json_free(lr);
            } else {
                free(es); prop = r; break;
            }
            free(es); json_free(r);
        }
        json_free(files);
    }
    if (prop) { json_free(scanned); json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey); return prop; }
    json_t *rsa = json_object(); json_set(rsa, "entries", scanned);
    json_t *resolved = skt_call(e, "resolve_skill_catalog", rsa);
    if (skt_is_raise(resolved)) { json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey); return resolved; }
    json_t *skills = json_array();
    for (size_t i = 0; i < json_len(resolved); i++) {
        json_t *s = json_get(resolved, i);
        if (!skt_is_obj(s)) { json_free(skills); json_free(resolved); json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey); return skt_attr_raise(s, "pop"); }
        json_t *vis = json_obj_get(s, "visible");
        if (!vis) { json_free(skills); json_free(resolved); json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey); return skt_keyerror_raise("visible"); }
        bool keep = skt_truth(vis) || include_hidden;
        json_obj_del(s, "visible");
        if (!keep) continue;
        json_t *da = json_object();
        json_set(da, "entry", json_copy(s));
        json_set(da, "disabled", json_copy(disabled));
        json_t *dis = skt_call(e, "is_disabled_entry", da);
        if (skt_is_raise(dis)) { json_free(skills); json_free(resolved); json_free(now); json_free(signature); json_free(disabled); json_free(roots); free(ckey); return dis; }
        bool disabled_entry = skt_truth(dis);
        json_free(dis);
        if (!disabled_entry) json_append(skills, json_copy(s));
    }
    json_free(resolved);
    json_t *entry = json_array();
    json_append(entry, signature);
    json_append(entry, now);
    json_append(entry, json_copy(skills));
    json_set(cache, ckey, entry);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(skills); i++) json_append(out, json_copy(json_get(skills, i)));
    json_free(skills); json_free(disabled); json_free(roots); free(ckey);
    return out;
}

/* PoP: skt_host_skills @ tools/skills_tool.py:_host_skills */
static json_t *skt_host_skills(skt_env *e, const json_t *in) {
    const char *fn_name = json_get_str(in, "fn_name", "");
    json_t *args = json_obj_get(in, "args");
    json_t *kwargs = json_obj_get(in, "kwargs");
    json_t *ha = json_object();
    json_set(ha, "feature", json_string("skills"));
    json_set(ha, "fn_name", json_string(fn_name));
    json_t *refusal = skt_call(e, "host_feature_refusal", ha);
    if (skt_is_raise(refusal)) return refusal;
    json_t *result;
    if (skt_truth(refusal)) {
        char *rs = skt_pystr(refusal);
        json_t *fa = json_object();
        json_set(fa, "error", json_string(rs));
        json_set(fa, "extra", json_object());
        free(rs);
        result = skt_call(e, "_fail", fa);
    } else {
        json_t *ca = json_object();
        json_set(ca, "name", json_string(fn_name));
        json_set(ca, "args", args ? json_copy(args) : json_array());
        json_set(ca, "kwargs", kwargs ? json_copy(kwargs) : json_object());
        result = skt_call(e, "fn_call", ca);
    }
    json_free(refusal);
    if (skt_is_raise(result)) return result;
    json_t *out = json_object();
    json_set(out, "name", json_string(fn_name));
    json_set(out, "result", result ? result : json_null());
    return out;
}

/* PoP: skt_legacy_portable_skill_name @ tools/skills_tool.py:_legacy_portable_skill_name */
static json_t *skt_legacy_portable_skill_name(skt_env *e, const json_t *pm, const char *namespace, const char *bare) {
    (void)e;
    if (strncmp(namespace, "agent-plugin-", 13) != 0) return NULL;
    json_t *plugins = json_obj_get(pm, "_plugins");
    json_t *keys = json_array();
    if (skt_is_obj(plugins)) {
        for (size_t i = 0; i < plugins->c.count; i++) {
            json_t *loaded = plugins->c.items[i];
            json_t *manifest = json_obj_get(loaded, "manifest");
            if (skt_truth(json_obj_get(manifest, "portable")) &&
                strcmp(json_get_str(manifest, "data_namespace", ""), namespace) == 0)
                json_append(keys, json_string(plugins->c.keys[i]));
        }
    }
    json_t *pskills = json_obj_get(pm, "_plugin_skills");
    json_t *out = NULL;
    if (skt_is_obj(pskills)) {
        for (size_t i = 0; i < pskills->c.count; i++) {
            json_t *entry = pskills->c.items[i];
            const char *pk = json_get_str(entry, "plugin_key", "");
            if (skt_arr_has_str(keys, pk) && strcmp(json_get_str(entry, "bare_name", ""), bare) == 0) {
                out = json_string(pskills->c.keys[i]);
                break;
            }
        }
    }
    json_free(keys);
    return out;
}
static json_t *skt_legacy_portable_skill_name_wrap(skt_env *e, const json_t *in) {
    json_t *pm = json_obj_get(in, "pm");
    return skt_legacy_portable_skill_name(e, pm, json_get_str(in, "namespace", ""), json_get_str(in, "bare", ""));
}

/* PoP: skt_resolve_plugin_skill @ tools/skills_tool.py:_resolve_plugin_skill */
static json_t *skt_resolve_plugin_skill(skt_env *e, const json_t *in) {
    char *name = skt_strdup(json_get_str(in, "name", ""));
    json_t *file_path = json_obj_get(in, "file_path");
    json_t *task_id = json_obj_get(in, "task_id");
    bool preprocess = json_get_bool(in, "preprocess", true);
    json_t *pa = json_object(); json_set(pa, "name", json_string(name));
    json_t *pq = skt_call(e, "parse_qualified_name", pa);
    if (skt_is_raise(pq)) { free(name); return pq; }
    char *ns = skt_strdup(skt_sval(json_get(pq, 0)));
    char *bare = skt_strdup(skt_sval(json_get(pq, 1)));
    json_free(pq);
    if (strcmp(ns, "plugin") == 0 && strchr(bare, ':')) {
        free(name); name = skt_strdup(bare);
        json_t *pa2 = json_object(); json_set(pa2, "name", json_string(name));
        json_t *pq2 = skt_call(e, "parse_qualified_name", pa2);
        if (skt_is_raise(pq2)) { free(name); free(ns); free(bare); return pq2; }
        free(ns); free(bare);
        ns = skt_strdup(skt_sval(json_get(pq2, 0)));
        bare = skt_strdup(skt_sval(json_get(pq2, 1)));
        json_free(pq2);
    }
    json_t *va = json_object(); json_set(va, "candidate", json_string(ns));
    json_t *valid = skt_call(e, "is_valid_namespace", va);
    if (skt_is_raise(valid)) { free(name); free(ns); free(bare); return valid; }
    if (!skt_truth(valid)) {
        json_free(valid);
        skt_buf b; skt_buf_init(&b);
        skt_buf_add(&b, "Invalid namespace '"); skt_buf_add(&b, ns);
        skt_buf_add(&b, "' in '"); skt_buf_add(&b, name);
        skt_buf_add(&b, "'. Namespaces must match [a-zA-Z0-9_-]+.");
        json_t *fa = json_object();
        json_set(fa, "error", json_string(b.s));
        json_set(fa, "extra", json_object());
        free(b.s);
        json_t *fail = skt_call(e, "_fail", fa);
        free(name); free(ns); free(bare);
        if (skt_is_raise(fail)) return fail;
        json_t *out = json_array(); json_append(out, fail); json_append(out, json_null());
        return out;
    }
    json_free(valid);
    json_t *dp = skt_seam0(e, "discover_plugins");
    if (skt_is_raise(dp)) { free(name); free(ns); free(bare); return dp; }
    json_free(dp);
    json_t *pm = skt_seam0(e, "get_plugin_manager");
    if (skt_is_raise(pm)) { free(name); free(ns); free(bare); return pm; }
    json_t *legacy = skt_legacy_portable_skill_name(e, pm, ns, bare);
    if (skt_truth(legacy)) {
        free(name); name = skt_strdup(skt_str(legacy));
        free(ns);
        char *colon = strchr(name, ':');
        ns = colon ? skt_slice(name, (size_t)(colon - name)) : skt_strdup(name);
    }
    json_free(legacy);
    json_t *amp = NULL;
    json_t *amp_r = skt_seam0(e, "_get_active_memory_provider");
    if (skt_is_raise(amp_r)) {
        char *amsg = skt_strdup(json_get_str(amp_r, "message", ""));
        if (skt_swallow_exc(amp_r)) { free(amsg); free(name); free(ns); free(bare); json_free(pm); return amp_r; }
        /* swallowed: skt_swallow_exc freed amp_r; log the message snapshot */
        json_t *la = skt_log_args1(amsg);
        free(amsg);
        json_t *lr = skt_log(e, "debug", "Failed pruning inactive memory-provider skills: %s", la, false);
        if (skt_is_raise(lr)) { free(name); free(ns); free(bare); json_free(pm); return lr; }
        json_free(lr);
    } else {
        amp = amp_r;
        json_t *pra = json_object(); json_set(pra, "provider", amp ? json_copy(amp) : json_null());
        json_t *pr = skt_call(e, "_prune_inactive_memory_provider_skills", pra);
        if (skt_is_raise(pr)) {
            char *pmsg = skt_strdup(json_get_str(pr, "message", ""));
            if (skt_swallow_exc(pr)) { free(pmsg); free(name); free(ns); free(bare); json_free(pm); json_free(amp); return pr; }
            /* swallowed: skt_swallow_exc freed pr; log the message snapshot */
            json_t *la = skt_log_args1(pmsg);
            free(pmsg);
            json_t *lr = skt_log(e, "debug", "Failed pruning inactive memory-provider skills: %s", la, false);
            if (skt_is_raise(lr)) { free(name); free(ns); free(bare); json_free(pm); json_free(amp); return lr; }
            json_free(lr);
        } else json_free(pr);
    }
    json_t *fa = json_object(); json_set(fa, "name", json_string(name));
    json_t *md = skt_call(e, "pm.find_plugin_skill", fa);
    if (skt_is_raise(md)) { free(name); free(ns); free(bare); json_free(pm); json_free(amp); return md; }
    if (skt_is_null(md) && skt_is_str(amp) && strcmp(skt_str(amp), ns) == 0) {
        json_t *la = json_object(); json_set(la, "namespace", json_string(ns));
        json_t *lm = skt_call(e, "load_memory_provider", la);
        if (skt_is_raise(lm)) {
            char *lmsg = skt_strdup(json_get_str(lm, "message", ""));
            if (skt_swallow_exc(lm)) { free(lmsg); free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return lm; }
            /* swallowed: skt_swallow_exc freed lm; log the message snapshot */
            json_t *largs = json_array();
            json_append(largs, json_string(ns)); json_append(largs, json_string(lmsg));
            free(lmsg);
            json_t *lr = skt_log(e, "debug", "Failed lazy memory-provider skill load for %s: %s", largs, false);
            if (skt_is_raise(lr)) { free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return lr; }
            json_free(lr);
        } else {
            json_free(lm);
            json_t *fa2 = json_object(); json_set(fa2, "name", json_string(name));
            json_t *md2 = skt_call(e, "pm.find_plugin_skill", fa2);
            if (skt_is_raise(md2)) { free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return md2; }
            json_free(md); md = md2;
        }
    }
    if (!skt_is_null(md)) {
        char *md_s = skt_pystr(md);
        json_t *ex = skt_path_bool(e, "exists", md_s);
        if (skt_is_raise(ex)) { free(md_s); free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return ex; }
        bool exists = skt_truth(ex);
        json_free(ex);
        if (!exists) {
            json_t *rma = json_object(); json_set(rma, "name", json_string(name));
            json_t *rm = skt_call(e, "pm.remove_plugin_skill", rma);
            if (skt_is_raise(rm)) { free(md_s); free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return rm; }
            json_free(rm);
            skt_buf b; skt_buf_init(&b);
            skt_buf_add(&b, "Skill '"); skt_buf_add(&b, name);
            skt_buf_add(&b, "' file no longer exists at "); skt_buf_add(&b, md_s);
            skt_buf_add(&b, ". The registry entry has been cleaned up \xe2\x80\x94 try again after the plugin is reloaded.");
            json_t *ffa = json_object();
            json_set(ffa, "error", json_string(b.s));
            json_set(ffa, "extra", json_object());
            free(b.s);
            json_t *fail = skt_call(e, "_fail", ffa);
            free(md_s); free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md);
            if (skt_is_raise(fail)) return fail;
            json_t *out = json_array(); json_append(out, fail); json_append(out, json_null());
            return out;
        }
        json_t *sa = json_object();
        json_set(sa, "skill_md", json_string(md_s));
        json_set(sa, "namespace", json_string(ns));
        json_set(sa, "bare", json_string(bare));
        json_set(sa, "file_path", file_path ? json_copy(file_path) : json_null());
        json_set(sa, "preprocess", json_bool(preprocess));
        json_set(sa, "session_id", task_id ? json_copy(task_id) : json_null());
        json_t *served = skt_call(e, "_serve_plugin_skill", sa);
        free(md_s); free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md);
        if (skt_is_raise(served)) return served;
        json_t *out = json_array(); json_append(out, served); json_append(out, json_null());
        return out;
    }
    json_t *lsa = json_object(); json_set(lsa, "namespace", json_string(ns));
    json_t *available = skt_call(e, "pm.list_plugin_skills", lsa);
    if (skt_is_raise(available)) { free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); return available; }
    if (skt_truth(available)) {
        json_t *avail_skills = json_array();
        for (size_t i = 0; i < json_len(available); i++) {
            char *q = skt_concat3(ns, ":", skt_sval(json_get(available, i)));
            json_append(avail_skills, json_string(q)); free(q);
        }
        char cnt[32]; snprintf(cnt, sizeof(cnt), "%zu", json_len(available));
        skt_buf hb; skt_buf_init(&hb);
        skt_buf_add(&hb, "The '"); skt_buf_add(&hb, ns);
        skt_buf_add(&hb, "' plugin provides "); skt_buf_add(&hb, cnt); skt_buf_add(&hb, " skill(s).");
        skt_buf eb; skt_buf_init(&eb);
        skt_buf_add(&eb, "Skill '"); skt_buf_add(&eb, bare);
        skt_buf_add(&eb, "' not found in plugin '"); skt_buf_add(&eb, ns); skt_buf_add(&eb, "'.");
        json_t *extra = json_object();
        json_set(extra, "available_skills", avail_skills);
        json_set(extra, "hint", json_string(hb.s));
        json_t *ffa = json_object();
        json_set(ffa, "error", json_string(eb.s));
        json_set(ffa, "extra", extra);
        free(hb.s); free(eb.s);
        json_t *fail = skt_call(e, "_fail", ffa);
        free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md); json_free(available);
        if (skt_is_raise(fail)) return fail;
        json_t *out = json_array(); json_append(out, fail); json_append(out, json_null());
        return out;
    }
    json_free(available);
    json_t *out = json_array();
    json_append(out, json_null());
    if (bare[0]) { char *lc = skt_concat3(ns, "/", bare); json_append(out, json_string(lc)); free(lc); }
    else json_append(out, json_null());
    free(name); free(ns); free(bare); json_free(pm); json_free(amp); json_free(md);
    return out;
}

/* PoP: skt_under_any @ tools/skills_tool.py:_under_any */
static json_t *skt_under_any(skt_env *e, const char *path, const json_t *dirs) {
    char *resolved = skt_strdup(path);
    json_t *rs = skt_path_resolve(e, path);
    if (skt_is_raise(rs)) {
        if (skt_swallow_exc(rs)) { free(resolved); return rs; }
    } else {
        free(resolved); resolved = skt_pystr(rs); json_free(rs);
    }
    bool any = false;
    for (size_t i = 0; i < json_len(dirs); i++)
        if (skt_is_relative_to(resolved, skt_sval(json_get(dirs, i)))) { any = true; break; }
    free(resolved);
    return json_bool(any);
}
static json_t *skt_under_any_wrap(skt_env *e, const json_t *in) {
    json_t *dirs = json_obj_get(in, "dirs");
    json_t *empty = json_array();
    json_t *r = skt_under_any(e, json_get_str(in, "path", ""), dirs ? dirs : empty);
    if (!dirs) json_free(empty);
    return r;
}

/* PoP: skt_is_package_owned_markdown @ tools/skills_tool.py:_is_package_owned_markdown */
static json_t *skt_is_package_owned_markdown(skt_env *e, const char *path, const char *search_root) {
    json_t *err = NULL;
    char *rel = skt_relative_to(path, search_root, &err);
    if (err) {
        if (skt_exc_is(skt_kind(err), "ValueError")) { json_free(err); return json_bool(false); }
        return err;
    }
    skt_parts rp = skt_parts_parse(rel);
    bool any = false;
    json_t *prop = NULL;
    for (size_t depth = 1; depth < rp.n; depth++) {
        skt_parts sub; sub.abs = false; sub.n = depth;
        sub.v = (char **)malloc(depth * sizeof(char *));
        for (size_t k = 0; k < depth; k++) sub.v[k] = skt_strdup(rp.v[k]);
        char *sub_s = skt_parts_str(&sub);
        skt_parts_free(&sub);
        char *joined = skt_path_join(search_root, sub_s);
        free(sub_s);
        char *cand = skt_path_join(joined, "SKILL.md");
        free(joined);
        json_t *isf = skt_path_bool(e, "is_file", cand);
        free(cand);
        if (skt_is_raise(isf)) { prop = isf; break; }
        bool f = skt_truth(isf);
        json_free(isf);
        if (f) { any = true; break; }
    }
    skt_parts_free(&rp);
    free(rel);
    if (prop) return prop;
    return json_bool(any);
}
static json_t *skt_is_package_owned_markdown_wrap(skt_env *e, const json_t *in) {
    return skt_is_package_owned_markdown(e, json_get_str(in, "path", ""), json_get_str(in, "search_root", ""));
}

/* PoP: skt_owning_search_dir @ tools/skills_tool.py:_owning_search_dir */
static json_t *skt_owning_search_dir(skt_env *e, const json_t *in) {
    (void)e;
    const char *skill_md = json_get_str(in, "skill_md", "");
    json_t *all_dirs = json_obj_get(in, "all_dirs");
    const char *best = NULL; size_t best_n = 0;
    for (size_t i = 0; i < json_len(all_dirs); i++) {
        const char *d = skt_sval(json_get(all_dirs, i));
        if (skt_is_relative_to(skill_md, d)) {
            size_t n = skt_parts_count(d);
            if (!best || n > best_n) { best = d; best_n = n; }
        }
    }
    return best ? json_string(best) : json_null();
}

/* PoP: skt_owned_relative @ tools/skills_tool.py:_owned_relative */
static json_t *skt_owned_relative(skt_env *e, const json_t *in) {
    const char *skill_md = json_get_str(in, "skill_md", "");
    json_t *skill_dir = json_obj_get(in, "skill_dir");
    json_t *all_dirs = json_obj_get(in, "all_dirs");
    json_t *oa = json_object();
    json_set(oa, "skill_md", json_string(skill_md));
    json_set(oa, "all_dirs", all_dirs ? json_copy(all_dirs) : json_array());
    json_t *root = skt_owning_search_dir(e, oa);
    json_free(oa);
    char *target;
    if (!skt_is_null(skill_dir)) target = skt_pystr(skill_dir);
    else {
        json_t *err = NULL;
        target = skt_with_suffix(skill_md, "", &err);
        if (err) { json_free(root); return err; }
    }
    json_t *out;
    if (!skt_is_null(root)) {
        json_t *err = NULL;
        char *rel = skt_relative_to(target, skt_str(root), &err);
        if (err) { free(target); json_free(root); return err; }
        out = json_string(rel); free(rel);
    } else out = json_string(target);
    free(target); json_free(root);
    return out;
}

/* _record helper: dedup by resolved path. */
static json_t *skt_record_candidate(skt_env *e, json_t *seen, json_t *candidates,
                                    const char *sd, const char *smd) {
    char *key = skt_strdup(smd);
    json_t *rs = skt_path_resolve(e, smd);
    if (skt_is_raise(rs)) {
        if (skt_swallow_exc(rs)) { free(key); return rs; }
    } else { free(key); key = skt_pystr(rs); json_free(rs); }
    if (!skt_arr_has_str(seen, key)) {
        skt_set_add(seen, key);
        json_t *pair = json_array();
        json_append(pair, sd ? json_string(sd) : json_null());
        json_append(pair, json_string(smd));
        json_append(candidates, pair);
    }
    free(key);
    return NULL;
}

/* PoP: skt_collect_skill_candidates @ tools/skills_tool.py:_collect_skill_candidates */
static json_t *skt_collect_skill_candidates(skt_env *e, const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    const char *lcn = json_get_str(in, "local_category_name", "");
    json_t *all_dirs = json_obj_get(in, "all_dirs");
    json_t *candidates = json_array();
    json_t *seen = json_array();
    json_t *prop = NULL;
    for (size_t di = 0; di < json_len(all_dirs) && !prop; di++) {
        const char *search_dir = skt_sval(json_get(all_dirs, di));
        const char *directs[2] = {name, lcn};
        for (int k = 0; k < 2 && !prop; k++) {
            const char *direct = directs[k];
            if (!direct || !direct[0]) continue;
            char *direct_path = skt_path_join(search_dir, direct);
            json_t *werr = NULL;
            char *flat = skt_with_suffix(direct_path, ".md", &werr);
            if (werr) { free(direct_path); prop = werr; break; }
            json_t *sa = json_object(); json_set(sa, "path", json_string(direct_path));
            json_t *sup = skt_call(e, "_is_skill_support_path", sa);
            if (skt_is_raise(sup)) { free(direct_path); free(flat); prop = sup; break; }
            bool support = skt_truth(sup);
            json_free(sup);
            bool taken = false;
            if (!support) {
                json_t *isd = skt_path_bool(e, "is_dir", direct_path);
                if (skt_is_raise(isd)) { free(direct_path); free(flat); prop = isd; break; }
                bool isdir = skt_truth(isd);
                json_free(isd);
                if (isdir) {
                    char *smd = skt_path_join(direct_path, "SKILL.md");
                    json_t *ex = skt_path_bool(e, "exists", smd);
                    if (skt_is_raise(ex)) { free(smd); free(direct_path); free(flat); prop = ex; break; }
                    bool exists = skt_truth(ex);
                    json_free(ex);
                    if (exists) {
                        json_t *err = skt_record_candidate(e, seen, candidates, direct_path, smd);
                        free(smd);
                        if (err) { free(direct_path); free(flat); prop = err; break; }
                        taken = true;
                    } else free(smd);
                }
            }
            if (!taken && !prop) {
                json_t *fex = skt_path_bool(e, "exists", flat);
                if (skt_is_raise(fex)) { free(direct_path); free(flat); prop = fex; break; }
                bool fexists = skt_truth(fex);
                json_free(fex);
                if (fexists) {
                    json_t *sa2 = json_object(); json_set(sa2, "path", json_string(flat));
                    json_t *sup2 = skt_call(e, "_is_skill_support_path", sa2);
                    if (skt_is_raise(sup2)) { free(direct_path); free(flat); prop = sup2; break; }
                    bool sup2v = skt_truth(sup2);
                    json_free(sup2);
                    if (!sup2v) {
                        json_t *pom = skt_is_package_owned_markdown(e, flat, search_dir);
                        if (skt_is_raise(pom)) { free(direct_path); free(flat); prop = pom; break; }
                        bool owned = skt_truth(pom);
                        json_free(pom);
                        if (!owned) {
                            json_t *err = skt_record_candidate(e, seen, candidates, NULL, flat);
                            if (err) { free(direct_path); free(flat); prop = err; break; }
                        }
                    }
                }
            }
            free(direct_path); free(flat);
        }
        if (prop) break;
        json_t *ia = json_object();
        json_set(ia, "dir", json_string(search_dir));
        json_set(ia, "filename", json_string("SKILL.md"));
        json_t *idx = skt_call(e, "iter_skill_index_files", ia);
        if (skt_is_raise(idx)) { prop = idx; break; }
        for (size_t i = 0; i < json_len(idx) && !prop; i++) {
            const char *found = skt_sval(json_get(idx, i));
            char *parent = skt_path_parent(found);
            char *pname = skt_path_name(parent);
            bool match = (strcmp(pname, name) == 0);
            if (!match) {
                json_t *sfa = json_object();
                json_set(sfa, "path", json_string(found));
                json_set(sfa, "content", json_null());
                json_t *sf = skt_call(e, "_safe_frontmatter", sfa);
                if (skt_is_raise(sf)) { free(parent); free(pname); prop = sf; break; }
                if (!skt_is_obj(sf)) { json_free(sf); free(parent); free(pname); prop = skt_attr_raise(sf, "get"); break; }
                match = (strcmp(json_get_str(sf, "name", ""), name) == 0);
                json_free(sf);
            }
            if (match) {
                json_t *err = skt_record_candidate(e, seen, candidates, parent, found);
                if (err) { free(parent); free(pname); prop = err; break; }
            }
            free(parent); free(pname);
        }
        json_free(idx);
        if (prop) break;
        char *pattern = skt_concat3(name, ".md", "");
        json_t *rg = skt_path_glob(e, "rglob", search_dir, pattern);
        free(pattern);
        if (skt_is_raise(rg)) { prop = rg; break; }
        for (size_t i = 0; i < json_len(rg) && !prop; i++) {
            const char *found = skt_sval(json_get(rg, i));
            char *fname = skt_path_name(found);
            bool ok = (strcmp(fname, "SKILL.md") != 0);
            free(fname);
            if (ok) {
                json_t *sa = json_object(); json_set(sa, "path", json_string(found));
                json_t *sup = skt_call(e, "_is_skill_support_path", sa);
                if (skt_is_raise(sup)) { prop = sup; break; }
                ok = !skt_truth(sup);
                json_free(sup);
            }
            if (ok) {
                json_t *pom = skt_is_package_owned_markdown(e, found, search_dir);
                if (skt_is_raise(pom)) { prop = pom; break; }
                ok = !skt_truth(pom);
                json_free(pom);
            }
            if (ok) {
                json_t *err = skt_record_candidate(e, seen, candidates, NULL, found);
                if (err) { prop = err; break; }
            }
        }
        json_free(rg);
    }
    json_free(seen);
    if (prop) { json_free(candidates); return prop; }
    return candidates;
}

/* PoP: skt_skill_linked_files @ tools/skills_tool.py:_skill_linked_files */
static json_t *skt_skill_linked_files(skt_env *e, const json_t *in) {
    json_t *skill_dir = json_obj_get(in, "skill_dir");
    json_t *files = json_object();
    if (skt_is_null(skill_dir)) return files;
    char *sd = skt_pystr(skill_dir);
    json_t *prop = NULL;
    for (size_t s = 0; s < sizeof(SKT_LINK_SPECS) / sizeof(SKT_LINK_SPECS[0]) && !prop; s++) {
        const skt_link_spec *spec = &SKT_LINK_SPECS[s];
        char *base = skt_path_join(sd, spec->sub);
        json_t *found = json_array();
        for (size_t g = 0; g < spec->ng && !prop; g++) {
            json_t *ex = skt_path_bool(e, "exists", base);
            if (skt_is_raise(ex)) { prop = ex; break; }
            bool exists = skt_truth(ex);
            json_free(ex);
            if (!exists) continue;
            json_t *gl = skt_path_glob(e, spec->recursive ? "rglob" : "glob", base, spec->globs[g]);
            if (skt_is_raise(gl)) { prop = gl; break; }
            for (size_t i = 0; i < json_len(gl); i++) {
                const char *f = skt_sval(json_get(gl, i));
                if (spec->files_only) {
                    json_t *isf = skt_path_bool(e, "is_file", f);
                    if (skt_is_raise(isf)) { prop = isf; break; }
                    bool isfile = skt_truth(isf);
                    json_free(isf);
                    if (!isfile) continue;
                }
                json_t *err = NULL;
                char *rel = skt_relative_to(f, sd, &err);
                if (err) { prop = err; break; }
                json_append(found, json_string(rel));
                free(rel);
            }
            json_free(gl);
        }
        if (prop) { free(base); json_free(found); break; }
        if (json_len(found) > 0) json_set(files, spec->sub, found);
        else json_free(found);
        free(base);
    }
    free(sd);
    if (prop) { json_free(files); return prop; }
    return files;
}

/* PoP: skt_skill_readiness @ tools/skills_tool.py:_skill_readiness */
static json_t *skt_skill_readiness(skt_env *e, const json_t *in) {
    json_t *fm = json_obj_get(in, "frontmatter");
    const char *skill_name = json_get_str(in, "skill_name", "");
    json_t *ga = json_object(); json_set(ga, "frontmatter", fm ? json_copy(fm) : json_null());
    json_t *req = skt_call(e, "_get_required_environment_variables", ga);
    if (skt_is_raise(req)) return req;
    json_t *ta = json_object();
    json_set(ta, "key", json_string("TERMINAL_ENV"));
    json_set(ta, "default", json_string("local"));
    json_t *te = skt_call(e, "terminal_env", ta);
    if (skt_is_raise(te)) { json_free(req); return te; }
    char *tes = skt_pystr(te);
    char *stripped = skt_strip(tes);
    char *lowered = skt_lower(stripped);
    char *backend = lowered[0] ? lowered : skt_strdup("local");
    free(tes); free(stripped);
    if (backend != lowered) free(lowered);
    json_free(te);
    json_t *env = skt_seam0(e, "load_env");
    if (skt_is_raise(env)) { json_free(req); free(backend); return env; }
    json_t *missing = json_array();
    json_t *prop = NULL;
    for (size_t i = 0; i < json_len(req) && !prop; i++) {
        json_t *ev = json_get(req, i);
        if (!skt_is_obj(ev)) { prop = skt_attr_raise(ev, "get"); break; }
        if (skt_truth(json_obj_get(ev, "optional"))) continue;
        json_t *nm = json_obj_get(ev, "name");
        if (!nm) { prop = skt_keyerror_raise("name"); break; }
        char *nms = skt_pystr(nm);
        json_t *pa = json_object();
        json_set(pa, "name", json_string(nms));
        json_set(pa, "snapshot", json_copy(env));
        json_t *pers = skt_call(e, "_is_env_var_persisted", pa);
        free(nms);
        if (skt_is_raise(pers)) { prop = pers; break; }
        bool persisted = skt_truth(pers);
        json_free(pers);
        if (!persisted) json_append(missing, json_copy(ev));
    }
    if (prop) { json_free(missing); json_free(req); json_free(env); free(backend); return prop; }
    json_t *ca = json_object();
    json_set(ca, "skill_name", json_string(skill_name));
    json_set(ca, "missing", json_copy(missing));
    json_t *capture = skt_call(e, "_capture_required_environment_variables", ca);
    if (skt_is_raise(capture)) { json_free(missing); json_free(req); json_free(env); free(backend); return capture; }
    if (json_len(missing) > 0) {
        json_t *env2 = skt_seam0(e, "load_env");
        if (skt_is_raise(env2)) { json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return env2; }
        json_free(env); env = env2;
    }
    json_t *mn = json_obj_get(capture, "missing_names");
    if (!mn) { json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return skt_keyerror_raise("missing_names"); }
    json_t *still = skt_norm_set(mn);
    json_t *remaining = json_array();
    for (size_t i = 0; i < json_len(req) && !prop; i++) {
        json_t *ev = json_get(req, i);
        if (!skt_is_obj(ev)) { prop = skt_attr_raise(ev, "get"); break; }
        if (skt_truth(json_obj_get(ev, "optional"))) continue;
        json_t *nm = json_obj_get(ev, "name");
        if (!nm) { prop = skt_keyerror_raise("name"); break; }
        char *nms = skt_pystr(nm);
        bool in_still = skt_arr_has_str(still, nms);
        bool keep = in_still;
        if (!in_still) {
            json_t *pa = json_object();
            json_set(pa, "name", json_string(nms));
            json_set(pa, "snapshot", json_copy(env));
            json_t *pers = skt_call(e, "_is_env_var_persisted", pa);
            if (skt_is_raise(pers)) { free(nms); prop = pers; break; }
            keep = !skt_truth(pers);
            json_free(pers);
        }
        if (keep) json_append(remaining, json_string(nms));
        free(nms);
    }
    if (prop) { json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return prop; }
    bool setup_needed = json_len(remaining) > 0;
    json_t *avail = json_array();
    for (size_t i = 0; i < json_len(req) && !prop; i++) {
        json_t *ev = json_get(req, i);
        json_t *nm = json_obj_get(ev, "name");
        if (!nm) { prop = skt_keyerror_raise("name"); break; }
        char *nms = skt_pystr(nm);
        if (!skt_arr_has_str(remaining, nms)) json_append(avail, json_string(nms));
        free(nms);
    }
    if (prop) { json_free(avail); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return prop; }
    json_t *refused = json_array();
    if (json_len(avail) > 0) {
        json_t *pa = json_object(); json_set(pa, "names", json_copy(avail));
        json_t *reg = skt_call(e, "register_env_passthrough", pa);
        if (skt_is_raise(reg)) {
            if (skt_swallow_exc(reg)) { json_free(avail); json_free(refused); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return reg; }
            /* swallowed: skt_swallow_exc freed reg */
            json_t *la = skt_log_args1(skill_name);
            json_t *lr = skt_log(e, "debug", "Could not register env passthrough for skill %s", la, true);
            if (skt_is_raise(lr)) { json_free(avail); json_free(refused); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return lr; }
            json_free(lr);
        } else {
            json_free(refused);
            refused = skt_is_arr(reg) ? reg : json_array();
            if (!skt_is_arr(reg)) json_free(reg);
        }
    }
    json_free(avail);
    json_t *cred_raw = json_obj_get(fm, "required_credential_files");
    json_t *missing_cred = json_array();
    if (skt_is_arr(cred_raw) && json_len(cred_raw) > 0) {
        json_t *cra = json_object(); json_set(cra, "files", json_copy(cred_raw));
        json_t *reg = skt_call(e, "register_credential_files", cra);
        if (skt_is_raise(reg)) {
            if (skt_swallow_exc(reg)) { json_free(missing_cred); json_free(refused); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return reg; }
            /* swallowed: skt_swallow_exc freed reg */
            json_t *la = skt_log_args1(skill_name);
            json_t *lr = skt_log(e, "debug", "Could not register credential files for skill %s", la, true);
            if (skt_is_raise(lr)) { json_free(missing_cred); json_free(refused); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return lr; }
            json_free(lr);
        } else {
            json_free(missing_cred);
            missing_cred = skt_is_arr(reg) ? reg : json_array();
            if (!skt_is_arr(reg)) json_free(reg);
            setup_needed = setup_needed || json_len(missing_cred) > 0;
        }
    }
    const char *status = setup_needed ? "setup_needed" : "available";
    json_t *fields = json_object();
    json_set(fields, "required_environment_variables", json_copy(req));
    json_set(fields, "required_commands", json_array());
    json_set(fields, "missing_required_environment_variables", json_copy(remaining));
    json_set(fields, "missing_credential_files", json_copy(missing_cred));
    json_set(fields, "missing_required_commands", json_array());
    json_set(fields, "setup_needed", json_bool(setup_needed));
    json_set(fields, "setup_skipped", json_copy(json_obj_get(capture, "setup_skipped")));
    json_set(fields, "readiness_status", json_string(status));
    json_t *extras = json_object();
    json_t *setup_help = NULL;
    for (size_t i = 0; i < json_len(req); i++) {
        json_t *h = json_obj_get(json_get(req, i), "help");
        if (skt_truth(h)) { setup_help = json_copy(h); break; }
    }
    if (setup_help) json_set(extras, "setup_help", json_copy(setup_help));
    json_t *gsh = json_obj_get(capture, "gateway_setup_hint");
    if (skt_truth(gsh)) json_set(extras, "gateway_setup_hint", json_copy(gsh));
    if (json_len(refused) > 0) {
        char *js = skt_join_strs(refused, ", ");
        skt_buf b; skt_buf_init(&b);
        skt_buf_add(&b, js);
        skt_buf_add(&b, " is a Hermes-managed credential and is never passed to the terminal or execute_code; commands in this skill that need it will not see it.");
        json_set(extras, "env_passthrough_refused", json_string(b.s));
        free(js); free(b.s);
    }
    json_t *missing_items = json_array();
    for (size_t i = 0; i < json_len(remaining); i++) {
        char *it = skt_concat3("env $", skt_sval(json_get(remaining, i)), "");
        json_append(missing_items, json_string(it)); free(it);
    }
    for (size_t i = 0; i < json_len(missing_cred); i++) {
        char *it = skt_concat3("file ", skt_sval(json_get(missing_cred, i)), "");
        json_append(missing_items, json_string(it)); free(it);
    }
    if (setup_needed) {
        json_t *ba = json_object();
        json_set(ba, "status", json_string(status));
        json_set(ba, "missing", json_copy(missing_items));
        json_set(ba, "setup_help", setup_help ? json_copy(setup_help) : json_null());
        json_t *note = skt_call(e, "_build_setup_note", ba);
        if (skt_is_raise(note)) { json_free(missing_items); json_free(setup_help); json_free(fields); json_free(extras); json_free(refused); json_free(missing_cred); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return note; }
        if (skt_truth(note)) {
            char *note_s = skt_pystr(note);
            json_t *rba = json_object(); json_set(rba, "backend", json_string(backend));
            json_t *remote = skt_call(e, "_is_remote_env_backend", rba);
            if (skt_is_raise(remote)) { free(note_s); json_free(note); json_free(missing_items); json_free(setup_help); json_free(fields); json_free(extras); json_free(refused); json_free(missing_cred); json_free(remaining); json_free(still); json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend); return remote; }
            bool is_remote = skt_truth(remote);
            json_free(remote);
            if (is_remote) {
                char *up = skt_upper(backend);
                skt_buf b; skt_buf_init(&b);
                skt_buf_add(&b, note_s); skt_buf_add(&b, " "); skt_buf_add(&b, up);
                skt_buf_add(&b, "-backed skills need these requirements available inside the remote environment as well.");
                free(note_s); note_s = skt_buf_take(&b); free(up);
            }
            json_set(extras, "setup_note", json_string(note_s));
            free(note_s);
        }
        json_free(note);
    }
    json_free(missing_items); json_free(setup_help);
    json_t *out = json_array();
    json_append(out, fields);
    json_append(out, extras);
    json_free(refused); json_free(missing_cred); json_free(remaining); json_free(still);
    json_free(capture); json_free(missing); json_free(req); json_free(env); free(backend);
    return out;
}

/* PoP: skt_locate_skill @ tools/skills_tool.py:locate_skill */
static json_t *skt_locate_skill(skt_env *e, const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *lcn = json_obj_get(in, "local_category_name");
    json_t *roots = json_obj_get(in, "roots");
    json_t *all_dirs = json_array();
    for (size_t i = 0; i < json_len(roots); i++) json_append(all_dirs, json_string(skt_sval(json_get(json_get(roots, i), 1))));
    if (json_len(all_dirs) == 0) {
        json_t *fa = json_object();
        json_set(fa, "error", json_string("Skills directory does not exist yet. It will be created on first install."));
        json_set(fa, "extra", json_object());
        json_t *fail = skt_call(e, "_fail", fa);
        json_free(all_dirs);
        if (skt_is_raise(fail)) return fail;
        json_t *out = json_array(); json_append(out, fail); json_append(out, json_null()); json_append(out, json_null());
        return out;
    }
    json_t *cca = json_object();
    json_set(cca, "name", json_string(name));
    json_set(cca, "local_category_name", lcn ? json_copy(lcn) : json_null());
    json_set(cca, "all_dirs", json_copy(all_dirs));
    json_t *candidates = skt_collect_skill_candidates(e, cca);
    json_free(cca);
    if (skt_is_raise(candidates)) { json_free(all_dirs); return candidates; }
    json_t *tier_of = json_object();
    for (size_t i = 0; i < json_len(roots); i++) {
        json_t *pair = json_get(roots, i);
        json_set(tier_of, skt_sval(json_get(pair, 1)), json_copy(json_get(pair, 0)));
    }
    json_t *prop = NULL;
    if (json_len(candidates) > 1) {
        json_t *owned_roots = json_array();
        for (size_t i = 0; i < json_len(candidates); i++) {
            json_t *oa = json_object();
            json_set(oa, "skill_md", json_string(skt_sval(json_get(json_get(candidates, i), 1))));
            json_set(oa, "all_dirs", json_copy(all_dirs));
            json_append(owned_roots, skt_owning_search_dir(e, oa));
            json_free(oa);
        }
        json_t *pick_args = json_array();
        for (size_t i = 0; i < json_len(candidates) && !prop; i++) {
            json_t *c = json_get(candidates, i);
            json_t *root = json_get(owned_roots, i);
            const char *smd = skt_sval(json_get(c, 1));
            json_t *tier = skt_is_null(root) ? NULL : json_obj_get(tier_of, skt_str(root));
            if (!tier) {
                char *rk = skt_is_null(root) ? skt_strdup("None") : skt_pyrepr(skt_str(root));
                prop = skt_raise_spec("KeyError", rk);
                free(rk); break;
            }
            json_t *ra = json_object();
            json_set(ra, "skill_md", json_string(smd));
            json_set(ra, "root", json_string(skt_str(root)));
            json_t *rank = skt_call(e, "skill_candidate_rank", ra);
            if (skt_is_raise(rank)) { prop = rank; break; }
            json_t *item = json_array();
            json_append(item, json_copy(tier));
            json_append(item, json_string(skt_str(root)));
            json_append(item, rank);
            json_append(item, json_string(smd));
            json_append(pick_args, item);
        }
        if (!prop) {
            json_t *pka = json_object(); json_set(pka, "candidates", pick_args);
            json_t *pick = skt_call(e, "pick_skill_candidate", pka);
            if (skt_is_raise(pick)) prop = pick;
            else {
                json_t *won = json_get(pick, 0);
                json_t *contenders = json_get(pick, 1);
                if (!skt_is_null(won)) {
                    size_t wi = (size_t)won->num_val;
                    json_t *dropped = json_array();
                    for (size_t i = 0; i < json_len(candidates); i++)
                        if (i != wi) json_append(dropped, json_string(skt_sval(json_get(json_get(candidates, i), 1))));
                    if (json_len(dropped) > 0) {
                        char *js = skt_join_strs(dropped, "; ");
                        json_t *la = json_array();
                        json_append(la, json_string(name));
                        json_append(la, json_string(skt_sval(json_get(json_get(candidates, wi), 1))));
                        json_append(la, json_string(js));
                        json_t *lr = skt_log(e, "info", "Skill '%s' resolved to %s by precedence (project > local > create_dir > external_dirs, then identical same-root copies); not loaded: %s", la, false);
                        free(js);
                        if (skt_is_raise(lr)) prop = lr;
                        else json_free(lr);
                    }
                    if (!prop) {
                        json_free(dropped);
                        json_t *winner = json_copy(json_get(candidates, wi));
                        json_free(candidates);
                        candidates = json_array();
                        json_append(candidates, winner);
                    } else json_free(dropped);
                } else {
                    json_t *nc = json_array();
                    for (size_t i = 0; i < json_len(contenders); i++) {
                        size_t ci = (size_t)json_get(contenders, i)->num_val;
                        json_append(nc, json_copy(json_get(candidates, ci)));
                    }
                    json_free(candidates);
                    candidates = nc;
                }
                json_free(pick);
            }
        } else json_free(pick_args);
        json_free(owned_roots);
    }
    if (!prop && json_len(candidates) > 1) {
        json_t *paths = json_array();
        for (size_t i = 0; i < json_len(candidates); i++) json_append(paths, json_string(skt_sval(json_get(json_get(candidates, i), 1))));
        json_t *load_set = json_array();
        for (size_t i = 0; i < json_len(candidates) && !prop; i++) {
            json_t *c = json_get(candidates, i);
            json_t *ora = json_object();
            json_set(ora, "skill_dir", json_copy(json_get(c, 0)));
            json_set(ora, "skill_md", json_string(skt_sval(json_get(c, 1))));
            json_set(ora, "all_dirs", json_copy(all_dirs));
            json_t *rel = skt_owned_relative(e, ora);
            json_free(ora);
            if (skt_is_raise(rel)) { prop = rel; break; }
            skt_set_add(load_set, skt_sval(rel));
            json_free(rel);
        }
        if (!prop) {
            json_t *load_names = json_array();
            for (size_t i = 0; i < json_len(load_set); i++) {
                const char *s = skt_sval(json_get(load_set, i));
                if (strcmp(s, name) != 0) json_append(load_names, json_string(s));
            }
            char *pj = skt_join_strs(paths, "; ");
            char cnt[32]; snprintf(cnt, sizeof(cnt), "%zu", json_len(candidates));
            json_t *la = json_array();
            json_append(la, json_string(name));
            json_append(la, json_number((double)json_len(candidates)));
            json_append(la, json_string(pj));
            json_t *lr = skt_log(e, "warning", "Skill name collision for '%s': %d candidates \xe2\x80\x94 %s", la, false);
            free(pj);
            if (skt_is_raise(lr)) prop = lr;
            else {
                json_free(lr);
                skt_buf b; skt_buf_init(&b);
                skt_buf_add(&b, SKT_AMBIGUOUS_PREFIX);
                skt_buf_add(&b, "'"); skt_buf_add(&b, name); skt_buf_add(&b, "': ");
                if (json_len(load_names) > 0) {
                    char *lj = skt_join_strs(load_names, ", ");
                    skt_buf_add(&b, "use one of "); skt_buf_add(&b, lj); skt_buf_add(&b, ". ");
                    free(lj);
                }
                skt_buf_add(&b, cnt);
                skt_buf_add(&b, " different skills share this name in the same skills directory tier; refusing to guess.");
                json_t *extra = json_object();
                json_set(extra, "matches", paths); paths = NULL;
                json_set(extra, "load_names", load_names); load_names = NULL;
                json_set(extra, "hint", json_string("Pass the exact relative path instead of the bare name, or rename one of the colliding skills so each name is unique."));
                json_t *fa = json_object();
                json_set(fa, "error", json_string(b.s));
                json_set(fa, "extra", extra);
                free(b.s);
                json_t *fail = skt_call(e, "_fail", fa);
                if (skt_is_raise(fail)) prop = fail;
                else {
                    json_t *out = json_array(); json_append(out, fail); json_append(out, json_null()); json_append(out, json_null());
                    json_free(candidates); json_free(tier_of); json_free(all_dirs);
                    return out;
                }
            }
            json_free(load_names);
        }
        json_free(load_set);
        json_free(paths);
    }
    if (prop) { json_free(candidates); json_free(tier_of); json_free(all_dirs); return prop; }
    json_t *skill_dir = NULL, *skill_md = NULL;
    if (json_len(candidates) > 0) {
        skill_dir = json_copy(json_get(json_get(candidates, 0), 0));
        skill_md = json_copy(json_get(json_get(candidates, 0), 1));
    }
    json_free(candidates);
    if (!skt_is_null(skill_md)) {
        json_t *oa = json_object();
        json_set(oa, "skill_md", json_copy(skill_md));
        json_set(oa, "all_dirs", json_copy(all_dirs));
        json_t *root = skt_owning_search_dir(e, oa);
        json_free(oa);
        json_t *tier = skt_is_null(root) ? NULL : json_obj_get(tier_of, skt_str(root));
        bool is_project = tier && skt_is_num(tier) && (int)tier->num_val == SKT_TIER_PROJECT;
        json_free(root);
        if (is_project) {
            json_t *qa = json_object(); json_set(qa, "skill_md", json_copy(skill_md));
            json_t *quar = skt_call(e, "is_quarantined_project_skill", qa);
            if (skt_is_raise(quar)) { json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs); return quar; }
            bool q = skt_truth(quar);
            json_free(quar);
            if (q) {
                skt_buf b; skt_buf_init(&b);
                skt_buf_add(&b, "Project skill '"); skt_buf_add(&b, name);
                skt_buf_add(&b, "' is quarantined: the security scan flagged its content as dangerous. It will not load until the repo's skill content changes and passes a re-scan.");
                json_t *extra = json_object();
                json_set(extra, "hint", json_string("Inspect the skill in the repo checkout, or untrust the repo with `hermes skills untrust`."));
                json_t *fa = json_object();
                json_set(fa, "error", json_string(b.s));
                json_set(fa, "extra", extra);
                free(b.s);
                json_t *fail = skt_call(e, "_fail", fa);
                json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs);
                if (skt_is_raise(fail)) return fail;
                json_t *out = json_array(); json_append(out, fail); json_append(out, json_null()); json_append(out, json_null());
                return out;
            }
        }
    }
    bool exists = false;
    if (!skt_is_null(skill_md)) {
        json_t *ex = skt_path_bool(e, "exists", skt_str(skill_md));
        if (skt_is_raise(ex)) { json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs); return ex; }
        exists = skt_truth(ex);
        json_free(ex);
    }
    if (skt_is_null(skill_md) || !exists) {
        json_t *ffa = json_object(); json_set(ffa, "skip_disabled", json_bool(false));
        json_t *fas = skt_call(e, "find_all_skills", ffa);
        if (skt_is_raise(fas)) { json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs); return fas; }
        json_t *ssa = json_object(); json_set(ssa, "skills", fas);
        json_t *sorted = skt_call(e, "sort_skills", ssa);
        if (skt_is_raise(sorted)) { json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs); return sorted; }
        json_t *available = json_array();
        for (size_t i = 0; i < json_len(sorted) && i < 20; i++) {
            json_t *nm = json_obj_get(json_get(sorted, i), "name");
            if (!nm) { json_free(available); json_free(sorted); json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs); return skt_keyerror_raise("name"); }
            json_append(available, json_copy(nm));
        }
        json_free(sorted);
        skt_buf b; skt_buf_init(&b);
        skt_buf_add(&b, "Skill '"); skt_buf_add(&b, name); skt_buf_add(&b, "' not found.");
        json_t *extra = json_object();
        json_set(extra, "available_skills", available);
        json_set(extra, "hint", json_string("Use skills_list to see all available skills"));
        json_t *fa = json_object();
        json_set(fa, "error", json_string(b.s));
        json_set(fa, "extra", extra);
        free(b.s);
        json_t *fail = skt_call(e, "_fail", fa);
        json_free(skill_dir); json_free(skill_md); json_free(tier_of); json_free(all_dirs);
        if (skt_is_raise(fail)) return fail;
        json_t *out = json_array(); json_append(out, fail); json_append(out, json_null()); json_append(out, json_null());
        return out;
    }
    json_free(tier_of); json_free(all_dirs);
    json_t *out = json_array();
    json_append(out, json_null());
    json_append(out, skill_dir ? skill_dir : json_null());
    json_append(out, skill_md ? skill_md : json_null());
    return out;
}

/* PoP: skt_log_security_warnings @ tools/skills_tool.py:_log_security_warnings */
static json_t *skt_log_security_warnings(skt_env *e, const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    const char *skill_md = json_get_str(in, "skill_md", "");
    const char *content = json_get_str(in, "content", "");
    json_t *all_dirs = json_obj_get(in, "all_dirs");
    const char *active = json_get_str(in, "active_skills_dir", "");
    json_t *trusted = json_array();
    json_t *ar = skt_path_resolve(e, active);
    if (skt_is_raise(ar)) { json_free(trusted); return ar; }
    { char *as = skt_pystr(ar); json_append(trusted, json_string(as)); free(as); json_free(ar); }
    for (size_t i = 0; i < json_len(all_dirs); i++) {
        json_t *dr = skt_path_resolve(e, skt_sval(json_get(all_dirs, i)));
        if (skt_is_raise(dr)) {
            if (skt_swallow_exc(dr)) { json_free(trusted); return dr; }
            break;
        }
        char *ds = skt_pystr(dr);
        json_append(trusted, json_string(ds));
        free(ds); json_free(dr);
    }
    json_t *warnings = json_array();
    json_t *ua = skt_under_any(e, skill_md, trusted);
    if (skt_is_raise(ua)) { json_free(warnings); json_free(trusted); return ua; }
    bool under = skt_truth(ua);
    json_free(ua);
    if (!under) {
        skt_buf b; skt_buf_init(&b);
        skt_buf_add(&b, "skill file is outside the trusted skills directory (~/.hermes/skills/): ");
        skt_buf_add(&b, skill_md);
        json_append(warnings, json_string(b.s)); free(b.s);
    }
    char *lc = skt_lower(content);
    bool inject = false;
    for (size_t i = 0; i < sizeof(SKT_INJECTION_PATTERNS) / sizeof(SKT_INJECTION_PATTERNS[0]); i++)
        if (strstr(lc, SKT_INJECTION_PATTERNS[i])) { inject = true; break; }
    free(lc);
    if (inject) json_append(warnings, json_string("skill content contains patterns that may indicate prompt injection"));
    if (json_len(warnings) > 0) {
        char *js = skt_join_strs(warnings, "; ");
        json_t *la = json_array();
        json_append(la, json_string(name));
        json_append(la, json_string(js));
        json_t *lr = skt_log(e, "warning", "Skill security warning for '%s': %s", la, false);
        free(js);
        if (skt_is_raise(lr)) { json_free(warnings); json_free(trusted); return lr; }
        json_free(lr);
    }
    json_free(warnings); json_free(trusted);
    return NULL;
}

/* ── dispatch table ───────────────────────────────────────────────────────── */
typedef struct { const char *name; json_t *(*fn)(skt_env *, const json_t *); } skt_dispatch;
static const skt_dispatch skt_table[] = {
    {"clear_skills_cache", skt_clear_skills_cache},
    {"_dir_tree_signature", skt_dir_tree_signature},
    {"bind_turn_secret_capture", skt_bind_turn_secret_capture},
    {"_skill_utils_delegate", skt_skill_utils_delegate},
    {"skill_search_dirs", skt_skill_search_dirs},
    {"_skill_catalog", skt_skill_catalog},
    {"_host_skills", skt_host_skills},
    {"_legacy_portable_skill_name", skt_legacy_portable_skill_name_wrap},
    {"_resolve_plugin_skill", skt_resolve_plugin_skill},
    {"_under_any", skt_under_any_wrap},
    {"_is_package_owned_markdown", skt_is_package_owned_markdown_wrap},
    {"_collect_skill_candidates", skt_collect_skill_candidates},
    {"_skill_linked_files", skt_skill_linked_files},
    {"_skill_readiness", skt_skill_readiness},
    {"_owning_search_dir", skt_owning_search_dir},
    {"locate_skill", skt_locate_skill},
    {"_owned_relative", skt_owned_relative},
    {"_log_security_warnings", skt_log_security_warnings},
};