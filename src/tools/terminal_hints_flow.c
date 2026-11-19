/*
 * terminal_hints_flow.c — tools/terminal_hints.py UNCLAIMED-remainder FLOW
 * (universal-seam convention, sibling of terminal_tool_flow2.c).
 *
 * Ports the UNCLAIMED callable units of the terminal-hints module:
 *   _regex_hint (factory), the nested `hint` closure (driven across its 7
 *   production _OUTPUT_HINTS instances + generic captures),
 *   _missing_command_hint (dict leaf), _wrapper_cd_hint (last-line fullmatch
 *   leaf), plus the module-constant regex matchers (_FAILURE_SHAPES.search,
 *   _MASKING_SHAPES[0/1].search, _WRAPPER_CD_FAILURE.fullmatch) as internal
 *   helpers. Claimed siblings (annotate_failure, annotate_masked_success,
 *   _first_token, the 7 thint_hint_* leaves) are SEAMED where reached.
 *
 * Every live regex call (re.compile, rx.search, rx.fullmatch) and every
 * custom-callable message invocation enters through th_seams_t.call(ctx,
 * name, args); args is OWNED by th_call (freed after the borrowed call);
 * results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every except site (the ported defs have NO try/except; the only catch in
 * the module lives in claimed annotate_failure). The exception-parent table
 * carries the FULL builtin hierarchy.
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static
 * th_*. String ops (rstrip, rsplit, slice, format) and the dict lookup are
 * transcribed inline (pure). ASCII scope; integral floats == ints.
 *
 * Modeling notes (differential oracle tests/oracle_terminal_hints_flow.py):
 *  - The hint closure's free var `message` is an explicit arg: a JSON string
 *    (str.format path) or {"__callable__":"missing_command_hint"} (inline real)
 *    or {"__callable__":"custom"} (seamed message_call). `rx` is SEAMED AWAY
 *    (rx.search records only {output}); the C needs no rx arg. The oracle drives
 *    the extracted closure with rx bound to a SimpleNamespace(search=recorder)
 *    double (a real re.Pattern cannot be monkeypatched).
 *  - re.compile is seamed (args {pattern, flags=INPUT}); the factory projects
 *    the closure to {pattern, message, flags} == its inputs (identity over the
 *    captures). Compiled rx.flags (32/40, re.UNICODE auto-added) never reach the
 *    trace.
 *  - rx.search / rx.fullmatch return a match descriptor {groups:[...]} or
 *    NULL (no match). Groups are the captured strings (group(1), group(2)...).
 *  - _wrapper_cd_hint: (output or "")[-4000:].rstrip().rsplit("\n",1)[-1]
 *    then fullmatch; the f-string is transcribed verbatim.
 *  - _missing_command_hint: _MISSING_COMMAND_HINTS.get(missing) or f-string.
 *  - Ordered collections: _MISSING_COMMAND_HINTS iterates in insertion order
 *    (python, pip). Production message literals are byte-exact (em-dash \xe2\x80\x94).
 *  - Fail-closed: absent io key -> RuntimeError "no script for <name>".
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "libjson/json.h"

typedef struct th_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} th_seams_t;

/* ── universal-seam helpers ── */
static json_t *th_call(const th_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool th_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *th_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *th_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *th_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *th_args(void) { return json_object(); }
static void th_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void th_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void th_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *th_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *th_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

/* ── exception-parent table (FULL builtin hierarchy) ── */
typedef struct { const char *kind; const char *parent; } th_exc_ent;
static const th_exc_ent TH_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"Warning", "Exception"},
    {"RuntimeError", "Exception"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FileNotFoundError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"FileExistsError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *th_exc_parent(const char *kind) {
    for (const th_exc_ent *e = TH_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, kind) == 0) return e->parent;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    return "Exception";
}
static bool th_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = th_exc_parent(k);
    }
    return false;
}
/* BaseException trio + CancelledError pass through except Exception */
static bool th_is_base_exc(const char *kind) {
    return !th_exc_is(kind, "Exception");
}

/* ── string helpers (pure, transcribed) ── */

/* Python str.rstrip() with no arg: strip trailing whitespace.
 * ASCII scope: space \t \n \r \v \f */
static size_t th_rstrip_len(const char *s, size_t len) {
    while (len > 0) {
        char c = s[len - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
            len--;
        else break;
    }
    return len;
}

/* Python s[-n:] slice: last n chars (n==0 -> whole string per -0==0). */
static const char *th_tail(const char *s, size_t len, size_t n, size_t *out_len) {
    if (n == 0 || n >= len) { *out_len = len; return s; }
    *out_len = n;
    return s + (len - n);
}

/* Python s.rsplit("\n", 1)[-1]: last line after final newline. */
static const char *th_last_line(const char *s, size_t len, size_t *out_len) {
    const char *nl = NULL;
    for (size_t i = len; i > 0; i--) {
        if (s[i - 1] == '\n') { nl = s + i; break; }
    }
    if (nl) { *out_len = len - (size_t)(nl - s); return nl; }
    *out_len = len; return s;
}

/* Python str.format(*groups) with {0},{1},... positional fields.
 * Only auto/manual positional indices used by the live messages.
 * Returns malloc'd string; caller frees. */
static char *th_format_groups(const char *fmt, const json_t *groups) {
    size_t cap = strlen(fmt) + 1;
    size_t ng = groups ? json_len(groups) : 0;
    for (size_t i = 0; i < ng; i++) {
        const json_t *g = json_get(groups, i);
        if (g && g->type == JSON_STRING) cap += strlen(g->str_val);
    }
    char *out = malloc(cap + 1);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = fmt;
    while (*p) {
        if (*p == '{' && p[1] >= '0' && p[1] <= '9') {
            /* parse index */
            size_t idx = 0;
            const char *q = p + 1;
            while (*q >= '0' && *q <= '9') { idx = idx * 10 + (size_t)(*q - '0'); q++; }
            if (*q == '}') {
                const json_t *g = (idx < ng) ? json_get(groups, idx) : NULL;
                const char *gs = (g && g->type == JSON_STRING) ? g->str_val : "";
                size_t gl = strlen(gs);
                memcpy(out + o, gs, gl); o += gl;
                p = q + 1;
                continue;
            }
        }
        if (*p == '{' && p[1] == '{') { out[o++] = '{'; p += 2; continue; }
        if (*p == '}' && p[1] == '}') { out[o++] = '}'; p += 2; continue; }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return out;
}

/* ── _MISSING_COMMAND_HINTS dict (insertion order: python, pip) ── */
static const char *th_missing_dict_get(const char *missing) {
    if (strcmp(missing, "python") == 0)
        return "This system has no bare `python` \xe2\x80\x94 use `python3`, or the project venv's "
               "interpreter (e.g. .venv/bin/python).";
    if (strcmp(missing, "pip") == 0)
        return "This system has no bare `pip` \xe2\x80\x94 use `pip3`, `python3 -m pip`, or the project venv's "
               "pip (e.g. .venv/bin/pip).";
    return NULL;
}

/* PoP: th_missing_command_hint @ tools/terminal_hints.py:_missing_command_hint */
static json_t *th_missing_command_hint(const char *missing) {
    const char *hit = th_missing_dict_get(missing);
    if (hit) return json_string(hit);
    /* f"`{missing}` is not installed or not on PATH. Verify with `which {missing}`; install it "
     * "or use an absolute path instead of retrying the same command." */
    size_t ml = strlen(missing);
    size_t cap = ml * 2 + 200;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    snprintf(buf, cap,
        "`%s` is not installed or not on PATH. Verify with `which %s`; install it "
        "or use an absolute path instead of retrying the same command.",
        missing, missing);
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

/* PoP: th_wrapper_cd_hint @ tools/terminal_hints.py:_wrapper_cd_hint */
static json_t *th_wrapper_cd_hint(const th_seams_t *s, const char *output) {
    /* last_line = (output or "")[-_SCAN_CHARS:].rstrip().rsplit("\n", 1)[-1] */
    const char *raw = output ? output : "";
    size_t rawlen = strlen(raw);
    size_t tlen;
    const char *tail = th_tail(raw, rawlen, 4000, &tlen);
    size_t rlen = th_rstrip_len(tail, tlen);
    size_t llen;
    const char *line = th_last_line(tail, rlen, &llen);
    /* copy line to NUL-terminated buffer */
    char *lbuf = malloc(llen + 1);
    if (!lbuf) return NULL;
    memcpy(lbuf, line, llen); lbuf[llen] = '\0';

    /* m = _WRAPPER_CD_FAILURE.fullmatch(last_line) */
    json_t *a = th_args();
    th_s(a, "last_line", lbuf);
    json_t *m = th_call(s, "rx.fullmatch", a);
    free(lbuf);
    if (th_raised(m)) return m;
    if (!m || m->type == JSON_NULL) { json_free(m); return NULL; }
    /* m.group(1), m.group(2) — copy before freeing m */
    const json_t *groups_b = th_oget(m, "groups");
    json_t *groups = groups_b ? json_copy(groups_b) : json_array();
    json_free(m);
    const char *g1 = (json_len(groups) > 0) ? th_sval(json_get(groups, 0)) : "";
    const char *g2 = (json_len(groups) > 1) ? th_sval(json_get(groups, 1)) : "";
    /* f-string verbatim */
    size_t cap = strlen(g1) + strlen(g2) + 400;
    char *buf = malloc(cap);
    if (!buf) { json_free(groups); return NULL; }
    snprintf(buf, cap,
        "The command never ran: Hermes could not enter its working directory %s "
        "(%s) before running it, so a `cd` inside the command cannot help. Pass "
        "`workdir` set to an accessible directory (on each call while this one stays unusable).",
        g1, g2);
    json_t *r = json_string(buf);
    free(buf);
    json_free(groups);
    return r;
}

/* PoP: th_regex_hint @ tools/terminal_hints.py:_regex_hint */
static json_t *th_regex_hint(const th_seams_t *s, const char *pattern,
                             const json_t *message, int flags) {
    /* rx = re.compile(pattern, flags) */
    json_t *a = th_args();
    th_s(a, "pattern", pattern);
    th_n(a, "flags", (double)flags);
    json_t *rx = th_call(s, "re.compile", a);
    if (th_raised(rx)) return rx;
    json_free(rx);
    /* return closure descriptor {pattern, message, flags} */
    json_t *desc = json_object();
    th_s(desc, "pattern", pattern);
    th_j(desc, "message", message);
    th_n(desc, "flags", (double)flags);
    return desc;
}

/* PoP: th_hint @ tools/terminal_hints.py:hint */
/* The nested closure (free vars rx/message bound per case): rx.search(output);
 * if m: message(m.group(1)) if callable(message) else message.format(*m.groups());
 * return None. rx is seamed away (rx.search); only message is an explicit arg. */
static json_t *th_hint(const th_seams_t *s, const json_t *message,
                       const char *command, const char *output) {
    (void)command;
    /* m = rx.search(output) */
    json_t *a = th_args();
    th_s(a, "output", output);
    json_t *m = th_call(s, "rx.search", a);
    if (th_raised(m)) return m;
    if (!m || m->type == JSON_NULL) { json_free(m); return NULL; }
    /* match found — copy groups BEFORE freeing m (ownership: groups borrowed) */
    const json_t *groups_b = th_oget(m, "groups");
    json_t *groups = groups_b ? json_copy(groups_b) : json_array();
    /* callable(message)? */
    const json_t *callable_tag = th_oget(message, "__callable__");
    if (callable_tag && callable_tag->type == JSON_STRING) {
        const char *cname = callable_tag->str_val;
        const char *g1 = (json_len(groups) > 0) ? th_sval(json_get(groups, 0)) : "";
        char *g1c = strdup(g1);
        json_free(groups);
        json_free(m);
        if (strcmp(cname, "missing_command_hint") == 0) {
            json_t *r = th_missing_command_hint(g1c);
            free(g1c);
            return r;
        }
        /* custom callable -> seam */
        json_t *ca = th_args();
        th_s(ca, "group1", g1c);
        free(g1c);
        json_t *cr = th_call(s, "message_call", ca);
        if (th_raised(cr)) return cr;
        return cr;
    }
    /* str message: message.format(*m.groups()) */
    const char *fmt = th_sval(message);
    json_free(m);
    char *formatted = th_format_groups(fmt, groups);
    json_free(groups);
    if (!formatted) return NULL;
    json_t *r = json_string(formatted);
    free(formatted);
    return r;
}

/* ── internal regex-constant matchers (no PoP; module Assign constants) ── */

/* _FAILURE_SHAPES.search(window) */
static json_t *th_failure_shapes_search(const th_seams_t *s, const char *window) {
    json_t *a = th_args();
    th_s(a, "window", window);
    return th_call(s, "failure_shapes.search", a);
}

/* _MASKING_SHAPES[0][0].search(cmd) — pipe shape */
static json_t *th_masking_pipe_search(const th_seams_t *s, const char *cmd) {
    json_t *a = th_args();
    th_s(a, "cmd", cmd);
    return th_call(s, "masking_pipe.search", a);
}

/* _MASKING_SHAPES[1][0].search(cmd) — || fallback shape */
static json_t *th_masking_fallback_search(const th_seams_t *s, const char *cmd) {
    json_t *a = th_args();
    th_s(a, "cmd", cmd);
    return th_call(s, "masking_fallback.search", a);
}

/* _WRAPPER_CD_FAILURE.fullmatch(last_line) — direct drive */
static json_t *th_wrapper_cd_fullmatch(const th_seams_t *s, const char *last_line) {
    json_t *a = th_args();
    th_s(a, "last_line", last_line);
    return th_call(s, "rx.fullmatch", a);
}

/* ── production _OUTPUT_HINTS instances (captured message; rx seamed) ──
 * The 7 closure instances from the live _OUTPUT_HINTS list. The C hardcodes
 * each captured message (byte-exact); the oracle extracts them from the live
 * module and binds the matching rx double + message. Driven as th_hint. */

/* instance: gh_unknown_json_field (pattern='Unknown JSON field: "?(\\w+)' flags=32) */
static json_t *th_hint_gh_unknown_json_field(const th_seams_t *s,
                                            const char *command, const char *output) {
    json_t *msg = json_string("The installed gh does not support the JSON field '{0}'. The valid field list is printed in the output above \xe2\x80\x94 retry using only fields from that list.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: merge_conflict (pattern='^CONFLICT |Automatic merge failed|needs merge' flags=40) */
static json_t *th_hint_merge_conflict(const th_seams_t *s,
                                     const char *command, const char *output) {
    json_t *msg = json_string("Git merge conflict. Do not retry this command. Resolve the conflicted files listed above (edit, then `git add`), then continue (`git rebase --continue` / commit the merge) \xe2\x80\x94 or abort with `--abort`.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: command_not_found (pattern='(?:bash: line \\d+: |bash: |sh: \\d*:? ?)?([\\w.+-]+): command not found' flags=32) */
static json_t *th_hint_command_not_found(const th_seams_t *s,
                                        const char *command, const char *output) {
    json_t *msg = json_object();
    th_s(msg, "__callable__", "missing_command_hint");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: module_not_found (pattern="(?:ModuleNotFoundError|ImportError): No module named '?([\\w.]+)" flags=32) */
static json_t *th_hint_module_not_found(const th_seams_t *s,
                                       const char *command, const char *output) {
    json_t *msg = json_string("Python cannot import '{0}'. Most often the wrong interpreter is running: activate the project venv (e.g. `source .venv/bin/activate`) or invoke its python directly. Only pip install if the package is genuinely absent from that venv.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: already_exists (pattern="(?:fatal|error):.*?'([^']+)' already exists" flags=32) */
static json_t *th_hint_already_exists(const th_seams_t *s,
                                     const char *command, const char *output) {
    json_t *msg = json_string("'{0}' already exists \xe2\x80\x94 retrying unchanged will keep failing. Reuse it, choose another name, or delete it first if it is genuinely stale.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: gh_rate_limit (pattern='API rate limit|was submitted too quickly' flags=32) */
static json_t *th_hint_gh_rate_limit(const th_seams_t *s,
                                    const char *command, const char *output) {
    json_t *msg = json_string("GitHub API rate limit hit \xe2\x80\x94 immediate retries will keep failing. Continue with other work and retry this operation later.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}

/* instance: permission_denied (pattern='Permission denied|EACCES' flags=32) */
static json_t *th_hint_permission_denied(const th_seams_t *s,
                                        const char *command, const char *output) {
    json_t *msg = json_string("Permission denied. Check ownership/mode of the target path (`ls -la`); prefer a user-writable location. Only escalate to sudo if the task genuinely requires it.");
    json_t *r = th_hint(s, msg, command, output);
    json_free(msg);
    return r;
}
