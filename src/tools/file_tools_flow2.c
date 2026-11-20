/*
 * file_tools_flow2.c — tools/file_tools.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/tools/file_tools.py
 *
 * PoP-grep first (CRLF-aware): every def already claimed by an existing PoP
 * annotation is SEAMED here, never re-ported. The claimed set (from
 * src/tools/file_tools*.c) includes read_file_tool, write_file_tool, patch_tool,
 * search_tool, _truncate_to_char_budget, _get_file_ops, clear_file_ops_cache,
 * _check_file_reqs, _is_expected_write_exception, _filter_read_blocked_search_results,
 * _rewrite_v4a_patch_paths_for_host, _file_ops_uses_host_paths, _get_max_read_chars,
 * _is_blocked_device, _special_file_kind, the _handle_* wrappers, etc. The
 * UNCLAIMED defs ported here (11 functions) are:
 *
 *   apply_char_budget         _apply_char_budget
 *   resolved_match_path       _resolved_match_path
 *   resolve_or_none           _resolve_or_none
 *   write_precheck_error      _write_precheck_error
 *   edit_warnings             _edit_warnings
 *   note_edited               _note_edited
 *   whole_file_rewrite_hint   _whole_file_rewrite_hint
 *   collect_v4a_header_paths  _collect_v4a_header_paths
 *   owned                     _owned
 *   read_device_base          _read_device_base
 *   dedup_stub_or_block       _dedup_stub_or_block
 *
 * Self-contained, libjson-only, no I/O. Every live collaborator enters through
 * ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call; `args` is a BORROWED JSON object (the seam
 * records a copy and must not free it); the result is a NEW json_t* the caller
 * frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. Exception class names
 * fold through the FULL builtin hierarchy (including aliases and 3.14 additions)
 * so `except (OSError, ValueError, RuntimeError)` / `except Exception` /
 * `except ValueError` catch exactly what live isinstance() catches; the
 * BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit,
 * BaseExceptionGroup) propagates through every catch site.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception.
 * _collect_v4a_header_paths / _dedup_stub_or_block wrap their JSON-string
 * return as {"__te__": <parsed object>} so the harness can compare it as a
 * structured value (the oracle json.loads() the live string the same way).
 *
 * Fixture domain (identical on both sides): ASCII text; POSIX lexical paths;
 * integral JSON numbers read as int; splitlines handles \n \r \r\n \v \f and
 * the ASCII record separators (non-ASCII separators are out of domain); the
 * rewrite-hint percentage fixtures avoid exact x.5 rounding boundaries;
 * in-function imports resolve silently (import failure is out of domain).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool ft2_jtruth(const json_t *v) {
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
static bool ft2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ft2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static char *ft2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
static void ft2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL). */
static void ft2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}
/* Python str() of a JSON scalar (domain: integral numbers, strings, null, bool). */
static char *ft2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.0e15) return ft2_xsf("%lld", (long long)d);
        return ft2_xsf("%.17g", d);
    }
    char *s = json_serialize(v);
    return s ? s : strdup("");
}
/* Python f"{n:,}" (non-negative domain). */
static char *ft2_comma(long long n) {
    char raw[64]; snprintf(raw, sizeof(raw), "%lld", n);
    bool neg = raw[0] == '-';
    const char *p = neg ? raw + 1 : raw;
    size_t L = strlen(p);
    size_t commas = L ? (L - 1) / 3 : 0;
    char *out = malloc(L + commas + (neg ? 1 : 0) + 1);
    size_t i = 0;
    if (neg) out[i++] = '-';
    /* Group from the RIGHT: first group is L%3 digits (or 3 when L%3==0). */
    size_t first = L % 3 ? L % 3 : 3;
    for (size_t k = 0; k < L; k++) {
        if (k && k >= first && (k - first) % 3 == 0) out[i++] = ',';
        out[i++] = p[k];
    }
    out[i] = '\0'; return out;
}
/* Python f"{ratio:.0%}" — half-even via nearbyint (domain avoids exact x.5 ties). */
static char *ft2_pct(double ratio) {
    long long pct = (long long)nearbyint(ratio * 100.0);
    return ft2_xsf("%lld%%", pct);
}
/* Python str.splitlines() count (ASCII boundary set). */
static long ft2_splitlines(const char *s, long *count) {
    long n = 0;
    const char *p = s ? s : "", *start = p;
    while (*p) {
        int br = 0, adv = 1;
        if (*p == '\n' || *p == '\v' || *p == '\f' ||
            *p == '\x1c' || *p == '\x1d' || *p == '\x1e') br = 1;
        else if (*p == '\r') { br = 1; if (p[1] == '\n') adv = 2; }
        if (br) { n++; p += adv; start = p; }
        else p++;
    }
    if (*start) n++;
    *count = n; return n;
}
/* Python repr() of a simple ASCII string (single-quoted). Domain: no quotes,
 * backslashes or control chars in V4A header paths. */
static char *ft2_pyrepr(const char *s) {
    return ft2_xsf("'%s'", s ? s : "");
}

/* ── universal seam ── */
typedef struct ft2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ft2_seams_t;

static json_t *ft2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool ft2_is_raise(const json_t *r) {
    return ft2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *ft2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Ownership: ft2_call OWNS args (frees it after the call); the seam BORROWS.
 * Results are NEW (caller frees). No seam installed -> fail-closed RuntimeError. */
static json_t *ft2_call(const ft2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) { json_free(args); return ft2_raise("RuntimeError", "no seam installed"); }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *ft2_args0(void) { return json_object(); }

/* ── FULL builtin exception hierarchy (child -> parent), Python 3.14 ──
 * The three OSError aliases (IOError/EnvironmentError/WindowsError) are folded
 * to OSError because CPython maps them to that class, not to Exception.
 * PythonFinalizationError (RuntimeError) and _IncompleteInputError (SyntaxError)
 * are the 3.14 additions. ExceptionGroup's second base is outside the domain. */
typedef struct ft2_exc_pair { const char *child; const char *parent; } ft2_exc_pair_t;
static const ft2_exc_pair_t FT2_EXC_TREE[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};
static const char *ft2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(FT2_EXC_TREE) / sizeof(FT2_EXC_TREE[0]); i++)
        if (!strcmp(FT2_EXC_TREE[i].child, c)) return FT2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool ft2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = ft2_exc_parent(c);
    }
    return false;
}

/* Constants mirrored from the quarry (tools/file_tools*.py). */
#define FT2_REWRITE_MIN_CHARS 20000L
#define FT2_REWRITE_MAX_CHARS 400000L
#define FT2_REWRITE_MIN_UNCHANGED 0.80
static const char *FT2_READ_DEDUP_STATUS_MESSAGE =
    "File unchanged since last read. The content from "
    "the earlier read_file result in this conversation is "
    "still current \342\200\224 refer to that instead of re-reading.";

/* ═══════════════════════ FLOW cores ═══════════════════════ */

/* PoP: ft2_apply_char_budget @ tools/file_tools.py:_apply_char_budget */
static json_t *ft2_apply_char_budget(const ft2_seams_t *s, json_t *result_dict,
                                     const char *content, long offset,
                                     const json_t *total_lines, long max_chars) {
    /* _truncate_to_char_budget is CLAIMED -> seam. ret = [trimmed, lines_kept, _]. */
    json_t *args = json_object();
    ft2_set_str(args, "content", content);
    json_set(args, "max_chars", json_number((double)max_chars));
    json_t *r = ft2_call(s, "truncate_to_char_budget", args);
    if (ft2_is_raise(r)) return r;
    const char *trimmed = "";
    long lines_kept = 0;
    if (r && r->type == JSON_ARRAY && json_len(r) >= 1 && ft2_is_str(json_get(r, 0)))
        trimmed = json_get(r, 0)->str_val;
    if (r && r->type == JSON_ARRAY && json_len(r) >= 2 && json_get(r, 1) &&
        json_get(r, 1)->type == JSON_NUMBER) lines_kept = (long)json_get(r, 1)->num_val;
    long next_offset = offset + lines_kept;

    json_set(result_dict, "content", json_string(trimmed));
    json_set(result_dict, "truncated", json_bool(true));
    json_set(result_dict, "truncated_by", json_string("bytes"));
    json_set(result_dict, "next_offset", json_number((double)next_offset));
    char *mc = ft2_comma(max_chars);
    char *tl = ft2_pystr(total_lines);
    char *hint = ft2_xsf(
        "Output truncated at the %s-char read budget after %ld line(s) "
        "(showing lines %ld-%ld of %s). Use offset=%ld to continue.",
        mc, lines_kept, offset, next_offset - 1, tl, next_offset);

    /* if len(trimmed.split("\n", 1)[0]) >= max_chars: */
    const char *nl = strchr(trimmed, '\n');
    size_t first_line = nl ? (size_t)(nl - trimmed) : strlen(trimmed);
    /* Live inserts "hint" BEFORE the optional "truncated_lines" key; key order
     * is semantic in canon(), so preserve it (updating an existing key keeps
     * its position). */
    json_set(result_dict, "hint", json_string(hint));
    if ((long)first_line >= max_chars) {
        char *hint2 = ft2_xsf(
            "%s Note: the first line alone exceeded the budget and was "
            "clamped mid-line; its remainder is not retrievable via offset.", hint);
        json_set(result_dict, "truncated_lines", json_bool(true));
        json_set(result_dict, "hint", json_string(hint2));
        free(hint2);
    }
    free(hint); free(tl); free(mc); json_free(r);

    json_t *out = json_object();
    json_set(out, "trimmed", json_string(trimmed));
    json_set(out, "result", json_copy(result_dict));
    return out;
}

/* PoP: ft2_resolved_match_path @ tools/file_tools.py:_resolved_match_path */
static json_t *ft2_resolved_match_path(const ft2_seams_t *s, const char *path, const char *task_id) {
    json_t *args = json_object();
    ft2_set_str(args, "path", path);
    ft2_set_str(args, "task_id", task_id);
    json_t *r = ft2_call(s, "resolve_path_for_task", args);
    if (ft2_is_raise(r)) {
        const char *c = ft2_raise_class(r);
        /* except (OSError, ValueError, RuntimeError): return path */
        if (ft2_cls_is(c, "OSError") || ft2_cls_is(c, "ValueError") || ft2_cls_is(c, "RuntimeError")) {
            json_free(r);
            return json_string(path ? path : "");
        }
        return r;  /* other / BaseException passes through */
    }
    json_t *o = json_string(r ? (ft2_is_str(r) ? (r->str_val ? r->str_val : "") : "None") : "None");
    json_free(r);
    return o;
}

/* PoP: ft2_resolve_or_none @ tools/file_tools.py:_resolve_or_none */
static json_t *ft2_resolve_or_none(const ft2_seams_t *s, const char *filepath,
                                   const char *task_id, bool entry) {
    json_t *args = json_object();
    ft2_set_str(args, "path", filepath);
    ft2_set_str(args, "task_id", task_id);
    json_t *r = ft2_call(s, entry ? "resolve_entry_for_task" : "resolve_path_for_task", args);
    if (ft2_is_raise(r)) {
        /* except Exception: return None (BaseException trio propagates). */
        if (ft2_cls_is(ft2_raise_class(r), "Exception")) { json_free(r); return NULL; }
        return r;
    }
    json_t *o = json_string(r ? (ft2_is_str(r) ? (r->str_val ? r->str_val : "") : "None") : "None");
    json_free(r);
    return o;
}

/* PoP: ft2_write_precheck_error @ tools/file_tools.py:_write_precheck_error */
static json_t *ft2_write_precheck_error(const ft2_seams_t *s, const json_t *paths,
                                        const json_t *content_paths, const char *task_id,
                                        bool cross_profile) {
    /* for p in paths: err = sensitive(p) or (None if cross else cross_profile(p)) */
    for (size_t i = 0; i < (paths && paths->type == JSON_ARRAY ? json_len(paths) : 0); i++) {
        const json_t *pv = json_get(paths, i);
        const char *p = ft2_is_str(pv) ? pv->str_val : "";
        json_t *args = json_object(); ft2_set_str(args, "path", p); ft2_set_str(args, "task_id", task_id);
        json_t *err = ft2_call(s, "check_sensitive_path", args);
        if (ft2_is_raise(err)) return err;
        if (ft2_jtruth(err)) return err;  /* `or` short-circuit */
        json_free(err);
        if (!cross_profile) {
            args = json_object(); ft2_set_str(args, "path", p); ft2_set_str(args, "task_id", task_id);
            err = ft2_call(s, "check_cross_profile_path", args);
            if (ft2_is_raise(err)) return err;
            if (ft2_jtruth(err)) return err;
            json_free(err);
        }
    }
    for (size_t i = 0; i < (content_paths && content_paths->type == JSON_ARRAY ? json_len(content_paths) : 0); i++) {
        const json_t *pv = json_get(content_paths, i);
        const char *p = ft2_is_str(pv) ? pv->str_val : "";
        json_t *args = json_object(); ft2_set_str(args, "path", p); ft2_set_str(args, "task_id", task_id);
        json_t *err = ft2_call(s, "check_binary_document_write", args);
        if (ft2_is_raise(err)) return err;
        if (ft2_jtruth(err)) return err;
        json_free(err);
    }
    /* return protected(paths) or approval(paths) */
    json_t *args = json_object();
    ft2_set_node(args, "paths", paths); ft2_set_str(args, "task_id", task_id);
    json_t *err = ft2_call(s, "check_protected_instruction_write", args);
    if (ft2_is_raise(err)) return err;
    if (ft2_jtruth(err)) return err;
    json_free(err);
    args = json_object();
    ft2_set_node(args, "paths", paths); ft2_set_str(args, "task_id", task_id);
    return ft2_call(s, "check_approval_required_write", args);
}

/* PoP: ft2_edit_warnings @ tools/file_tools.py:_edit_warnings */
static json_t *ft2_edit_warnings(const ft2_seams_t *s, const json_t *paths,
                                 const json_t *path_to_resolved, const char *task_id) {
    json_t *warnings = json_array();
    for (size_t i = 0; i < (paths && paths->type == JSON_ARRAY ? json_len(paths) : 0); i++) {
        const json_t *pv = json_get(paths, i);
        const char *p = ft2_is_str(pv) ? pv->str_val : "";
        const json_t *rv = path_to_resolved ? json_obj_get(path_to_resolved, p) : NULL;
        json_t *w = NULL;
        bool have_r = rv && ft2_jtruth(rv);
        /* w = (file_state.check_stale(task_id, r) if r else None) or check_file_staleness(p, task_id) */
        if (have_r) {
            json_t *args = json_object(); ft2_set_str(args, "task_id", task_id);
            ft2_set_node(args, "path", rv);
            w = ft2_call(s, "file_state.check_stale", args);
            if (ft2_is_raise(w)) { json_free(warnings); return w; }
        }
        if (!w || !ft2_jtruth(w)) {
            if (w) json_free(w);
            json_t *args = json_object(); ft2_set_str(args, "path", p); ft2_set_str(args, "task_id", task_id);
            w = ft2_call(s, "check_file_staleness", args);
            if (ft2_is_raise(w)) { json_free(warnings); return w; }
        }
        /* if not w and r: w = _path_resolution_warning(p, Path(r), task_id) */
        if ((!w || !ft2_jtruth(w)) && have_r) {
            if (w) json_free(w);
            json_t *args = json_object(); ft2_set_str(args, "path", p);
            ft2_set_node(args, "resolved", rv); ft2_set_str(args, "task_id", task_id);
            w = ft2_call(s, "path_resolution_warning", args);
            if (ft2_is_raise(w)) { json_free(warnings); return w; }
        }
        if (w && ft2_jtruth(w)) json_append(warnings, w);
        else if (w) json_free(w);
    }
    return warnings;
}

/* PoP: ft2_note_edited @ tools/file_tools.py:_note_edited */
static json_t *ft2_note_edited(const ft2_seams_t *s, const char *task_id,
                               const json_t *paths, const json_t *path_to_resolved,
                               const json_t *session_id) {
    /* resolved_paths = [path_to_resolved.get(p) or p for p in paths] */
    json_t *rp = json_array();
    for (size_t i = 0; i < (paths && paths->type == JSON_ARRAY ? json_len(paths) : 0); i++) {
        const json_t *pv = json_get(paths, i);
        const char *p = ft2_is_str(pv) ? pv->str_val : "";
        const json_t *rv = path_to_resolved ? json_obj_get(path_to_resolved, p) : NULL;
        if (rv && ft2_jtruth(rv)) json_append(rp, json_copy(rv));
        else json_append(rp, json_string(p));
    }
    json_t *args = json_object(); ft2_set_str(args, "task_id", task_id);
    json_set(args, "paths", rp); ft2_set_node(args, "session_id", session_id);
    json_t *r = ft2_call(s, "mark_verification_stale", args);
    if (ft2_is_raise(r)) return r;
    json_free(r);
    for (size_t i = 0; i < (paths && paths->type == JSON_ARRAY ? json_len(paths) : 0); i++) {
        const json_t *pv = json_get(paths, i);
        const char *p = ft2_is_str(pv) ? pv->str_val : "";
        args = json_object(); ft2_set_str(args, "path", p); ft2_set_str(args, "task_id", task_id);
        r = ft2_call(s, "update_read_timestamp", args);
        if (ft2_is_raise(r)) return r;
        json_free(r);
        const json_t *rv = path_to_resolved ? json_obj_get(path_to_resolved, p) : NULL;
        if (rv && ft2_jtruth(rv)) {
            args = json_object(); ft2_set_str(args, "task_id", task_id); ft2_set_node(args, "path", rv);
            r = ft2_call(s, "file_state.note_write", args);
            if (ft2_is_raise(r)) return r;
            json_free(r);
        }
    }
    return NULL;  /* None */
}

/* PoP: ft2_whole_file_rewrite_hint @ tools/file_tools.py:_whole_file_rewrite_hint */
static json_t *ft2_whole_file_rewrite_hint(const ft2_seams_t *s, const char *task_id,
                                           const char *resolved, const char *new_content) {
    if (!resolved || !*resolved) return NULL;
    long nc = (long)strlen(new_content ? new_content : "");
    if (nc < FT2_REWRITE_MIN_CHARS || nc > FT2_REWRITE_MAX_CHARS) return NULL;
    json_t *args = json_object(); ft2_set_str(args, "task_id", task_id); ft2_set_str(args, "resolved", resolved);
    json_t *r = ft2_call(s, "read_file_raw", args);
    if (ft2_is_raise(r)) {
        /* live: try: result = ...read_file_raw(...) ... except Exception: return None
         * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) propagates. */
        if (ft2_cls_is(ft2_raise_class(r), "Exception")) { json_free(r); return NULL; }
        return r;
    }
    const json_t *old = ft2_is_obj(r) ? json_obj_get(r, "content") : NULL;
    const json_t *err = ft2_is_obj(r) ? json_obj_get(r, "error") : NULL;
    if ((err && ft2_jtruth(err)) || !ft2_is_str(old)) { json_free(r); return NULL; }
    const char *olds = old->str_val ? old->str_val : "";
    long oc = (long)strlen(olds);
    if (oc < FT2_REWRITE_MIN_CHARS || oc > FT2_REWRITE_MAX_CHARS) { json_free(r); return NULL; }
    long old_n = 0, new_n = 0;
    ft2_splitlines(olds, &old_n); ft2_splitlines(new_content ? new_content : "", &new_n);
    if (old_n == 0) { json_free(r); return NULL; }
    /* Counter(old_lines) & Counter(new_lines): map line -> count, take min. */
    json_t *co = json_object(), *cn = json_object();
    const char *p = olds;
    while (*p) {
        const char *st = p;
        while (*p && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f' &&
               *p != '\x1c' && *p != '\x1d' && *p != '\x1e') p++;
        char *line = strndup(st, (size_t)(p - st));
        const json_t *cur = json_obj_get(co, line);
        json_set(co, line, json_number(cur ? cur->num_val + 1 : 1));
        free(line);
        if (*p == '\r' && p[1] == '\n') p += 2; else if (*p) p++;
    }
    p = new_content ? new_content : "";
    while (*p) {
        const char *st = p;
        while (*p && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f' &&
               *p != '\x1c' && *p != '\x1d' && *p != '\x1e') p++;
        char *line = strndup(st, (size_t)(p - st));
        const json_t *cur = json_obj_get(cn, line);
        json_set(cn, line, json_number(cur ? cur->num_val + 1 : 1));
        free(line);
        if (*p == '\r' && p[1] == '\n') p += 2; else if (*p) p++;
    }
    long unchanged = 0;
    for (size_t i = 0; i < co->c.count; i++) {
        const json_t *cv = json_obj_get(cn, co->c.keys[i]);
        if (cv) {
            double a = co->c.items[i]->num_val, b = cv->num_val;
            unchanged += (long)(a < b ? a : b);
        }
    }
    json_free(co); json_free(cn); json_free(r);
    long denom = old_n > new_n ? old_n : new_n;
    double ratio = (double)unchanged / (double)denom;
    if (ratio < FT2_REWRITE_MIN_UNCHANGED) return NULL;
    long changed = denom - unchanged;
    char *u = ft2_comma(unchanged), *nl = ft2_comma(new_n), *ch = ft2_comma(changed);
    char *ncc = ft2_comma(nc), *pct = ft2_pct(ratio);
    char *hint = ft2_xsf(
        "%s of %s lines were already on disk (%s unchanged); ~%s "
        "line(s) actually changed. Re-sending a %s-char file costs output "
        "tokens for every unchanged line; for edits like this use patch "
        "(old_string/new_string), which sends only the changed region.",
        u, nl, pct, ch, ncc);
    free(u); free(nl); free(ch); free(ncc); free(pct);
    json_t *out = json_string(hint); free(hint); return out;
}

/* PoP: ft2_collect_v4a_header_paths @ tools/file_tools.py:_collect_v4a_header_paths */
static json_t *ft2_collect_v4a_header_paths(const ft2_seams_t *s, const char *patch) {
    /* Walk the patch line by line. SINGLE headers first (in order), then MOVE
     * headers (in order), matching _V4A_SINGLE_HEADER_RE / _V4A_MOVE_HEADER_RE
     * with re.MULTILINE. */
    json_t *single = json_array();  /* [ [path, writes_text_bool], ... ] */
    json_t *moves = json_array();   /* [ [src, dst], ... ] */
    const char *p = patch ? patch : "";
    while (*p) {
        const char *line = p;
        const char *le = strchr(p, '\n');
        size_t ll = le ? (size_t)(le - p) : strlen(p);
        char *L = strndup(line, ll);
        /* skip leading "***" + \s* */
        const char *q = L;
        if (!strncmp(q, "***", 3)) {
            q += 3;
            while (*q && (*q == ' ' || *q == '\t')) q++;
            const char *op = NULL;
            if (!strncmp(q, "Update", 6)) op = "Update";
            else if (!strncmp(q, "Add", 3)) op = "Add";
            else if (!strncmp(q, "Delete", 6)) op = "Delete";
            if (op) {
                const char *w = q + strlen(op);
                while (*w == ' ' || *w == '\t') w++;
                if (!strncmp(w, "File:", 5)) {
                    w += 5;
                    while (*w == ' ' || *w == '\t') w++;
                    if (*w) {
                        json_t *h = json_array(); json_append(h, json_string(w));
                        json_append(h, json_bool(!strcmp(op, "Update") || !strcmp(op, "Add")));
                        json_append(single, h);
                    }
                }
            } else if (!strncmp(q, "Move", 4)) {
                const char *w = q + 4;
                while (*w == ' ' || *w == '\t') w++;
                if (!strncmp(w, "File:", 5)) {
                    w += 5;
                    const char *arrow = strstr(w, "->");
                    if (arrow) {
                        char *src = strndup(w, (size_t)(arrow - w));
                        size_t sl = strlen(src);
                        while (sl && (src[sl - 1] == ' ' || src[sl - 1] == '\t')) src[--sl] = '\0';
                        const char *dst = arrow + 2;
                        while (*dst == ' ' || *dst == '\t') dst++;
                        if (*src && *dst) {
                            /* Python yields BOTH endpoints as separate headers. */
                            json_t *h1 = json_array(); json_append(h1, json_string(src));
                            json_append(h1, json_bool(false)); json_append(moves, h1);
                            json_t *h2 = json_array(); json_append(h2, json_string(dst));
                            json_append(h2, json_bool(false)); json_append(moves, h2);
                        }
                        free(src);
                    }
                }
            }
        }
        free(L);
        if (!le) break;
        p = le + 1;
    }

    json_t *paths = json_array(), *content_paths = json_array(), *entry_paths = json_array();
    for (int pass = 0; pass < 2; pass++) {
        json_t *src = pass == 0 ? single : moves;
        for (size_t i = 0; i < json_len(src); i++) {
            json_t *h = json_get(src, i);
            const char *raw = json_get(h, 0)->str_val;
            /* v4a_path = raw.strip() */
            const char *a = raw; while (*a == ' ' || *a == '\t' || *a == '\r') a++;
            size_t z = strlen(a); while (z && (a[z-1] == ' ' || a[z-1] == '\t' || a[z-1] == '\r')) z--;
            char *v4a = strndup(a, z);
            json_t *args = json_object(); ft2_set_str(args, "path", v4a);
            json_t *trav = ft2_call(s, "has_traversal_component", args);
            if (ft2_is_raise(trav)) { free(v4a); json_free(paths); json_free(content_paths);
                json_free(entry_paths); json_free(single); json_free(moves); return trav; }
            if (ft2_jtruth(trav)) {
                char *rrepr = ft2_pyrepr(v4a);
                char *msg = ft2_xsf(
                    "V4A patch header contains '..' traversal: %s. "
                    "Use the agent's cwd-relative path (no '..') or an absolute "
                    "path in '*** Update File:' / '*** Add File:' / "
                    "'*** Delete File:' / '*** Move File:' headers.", rrepr);
                json_t *te = json_object(); ft2_set_str(te, "error", msg);
                free(msg); free(rrepr); free(v4a); json_free(trav);
                json_free(paths); json_free(content_paths); json_free(entry_paths);
                json_free(single); json_free(moves);
                json_t *out = json_object(); json_set(out, "__te__", te); return out;
            }
            json_free(trav);
            json_append(paths, json_string(v4a));
            if (pass == 0) {
                if (json_get(h, 1)->bool_val) json_append(content_paths, json_string(v4a));
                else json_append(entry_paths, json_string(v4a));
            } else {
                json_append(entry_paths, json_string(v4a));  /* both move endpoints */
            }
            free(v4a);
        }
    }
    json_free(single); json_free(moves);
    json_t *out = json_object();
    json_set(out, "paths", paths);
    json_set(out, "content_paths", content_paths);
    json_set(out, "entry_paths", entry_paths);
    return out;
}

/* PoP: ft2_owned @ tools/file_tools.py:_owned */
static json_t *ft2_owned(const ft2_seams_t *s, const char *owner, const json_t *call_kwargs) {
    /* with bind_owner(owner): return fn(**call_kwargs)
     * The CM and the called fn are seamed; __exit__ always fires and never
     * suppresses (bind_owner is not a suppressing CM). */
    json_t *args = json_object(); ft2_set_str(args, "owner", owner);
    json_t *r = ft2_call(s, "bind_owner_enter", args);
    if (ft2_is_raise(r)) return r;
    json_free(r);
    args = json_object(); ft2_set_str(args, "owner", owner);
    if (call_kwargs && call_kwargs->type == JSON_OBJECT)
        for (size_t i = 0; i < call_kwargs->c.count; i++)
            json_set(args, call_kwargs->c.keys[i], json_copy(call_kwargs->c.items[i]));
    r = ft2_call(s, "owned_call", args);
    args = json_object(); ft2_set_str(args, "owner", owner);
    if (ft2_is_raise(r)) ft2_set_str(args, "exc", ft2_raise_class(r));
    else json_set(args, "exc", json_null());
    json_t *x = ft2_call(s, "bind_owner_exit", args);  /* return ignored (no suppress) */
    json_free(x);
    return r;
}

/* PoP: ft2_read_device_base @ tools/file_tools.py:_read_device_base */
static json_t *ft2_read_device_base(const ft2_seams_t *s, const char *path, const char *task_id) {
    json_t *args = json_object(); ft2_set_str(args, "path", path);
    json_t *e = ft2_call(s, "path_expanduser", args);
    if (ft2_is_raise(e)) {
        if (ft2_cls_is(ft2_raise_class(e), "ValueError")) {
            json_t *h = ft2_call(s, "hosted_scope", ft2_args0());
            if (ft2_is_raise(h)) { json_free(e); return h; }
            bool hosted = ft2_jtruth(h); json_free(h);
            if (!hosted) return e;  /* re-raise the original ValueError */
            json_free(e);
            json_t *o = json_object(); json_set(o, "base", json_null());
            json_set(o, "unanchored", json_bool(true));
            return o;
        }
        return e;  /* other exception passes through */
    }
    const char *expanded = ft2_is_str(e) ? (e->str_val ? e->str_val : "") : "";
    bool is_abs = expanded[0] == '/';
    json_free(e);
    json_t *o = json_object();
    if (is_abs) json_set(o, "base", json_null());
    else {
        args = json_object(); ft2_set_str(args, "task_id", task_id);
        json_t *b = ft2_call(s, "resolve_base_dir", args);
        if (ft2_is_raise(b)) { json_free(o); return b; }
        ft2_set_node(o, "base", b);
        json_free(b);
    }
    json_set(o, "unanchored", json_bool(false));
    return o;
}

/* PoP: ft2_dedup_stub_or_block @ tools/file_tools.py:_dedup_stub_or_block */
static json_t *ft2_dedup_stub_or_block(const ft2_seams_t *s, json_t *task_data,
                                       const json_t *dedup_key, const char *path) {
    /* dedup_key = (path, offset, limit); canonical string key shared with oracle. */
    const char *kp = (dedup_key && json_len(dedup_key) >= 1 && ft2_is_str(json_get(dedup_key, 0)))
                     ? json_get(dedup_key, 0)->str_val : "";
    long off = (dedup_key && json_len(dedup_key) >= 2 && json_get(dedup_key, 1)) ? (long)json_get(dedup_key, 1)->num_val : 0;
    long lim = (dedup_key && json_len(dedup_key) >= 3 && json_get(dedup_key, 2)) ? (long)json_get(dedup_key, 2)->num_val : 0;
    char *key = ft2_xsf("%s|%ld|%ld", kp, off, lim);
    json_t *dh = json_obj_get(task_data, "dedup_hits");
    long hits = 0;
    if (dh && dh->type == JSON_OBJECT) {
        const json_t *hv = json_obj_get(dh, key);
        if (hv && hv->type == JSON_NUMBER) hits = (long)hv->num_val;
    }
    hits++;
    if (!dh || dh->type != JSON_OBJECT) { dh = json_object(); json_set(task_data, "dedup_hits", dh); }
    json_set(dh, key, json_number((double)hits));
    json_t *args = json_object(); ft2_set_node(args, "task_data", task_data);
    json_t *r = ft2_call(s, "cap_read_tracker_data", args);
    if (ft2_is_raise(r)) { free(key); return r; }
    json_free(r);

    json_t *te = json_object();
    if (hits >= 2) {
        char *msg = ft2_xsf(
            "BLOCKED: You have called read_file on this exact region %ld times and "
            "the file has NOT changed. STOP calling read_file for this path \342\200\224 "
            "the content from your earlier read_file result in this conversation is "
            "still current. Proceed with your task using the information you already have.",
            hits + 1);
        ft2_set_str(te, "error", msg);
        ft2_set_str(te, "path", path);
        json_set(te, "already_read", json_number((double)(hits + 1)));
        json_set(te, "guardrail_refusal", json_bool(true));
        free(msg);
    } else {
        ft2_set_str(te, "status", "unchanged");
        ft2_set_str(te, "message", FT2_READ_DEDUP_STATUS_MESSAGE);
        ft2_set_str(te, "path", path);
        json_set(te, "dedup", json_bool(true));
        json_set(te, "content_returned", json_bool(false));
    }
    free(key);
    json_t *out = json_object(); json_set(out, "__te__", te); return out;
}
