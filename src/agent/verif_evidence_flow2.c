/*
 * verif_evidence_flow2.c -- FLOW-oracle port of the UNCLAIMED remainder of
 * agent/verification_evidence.py (Slermes C11 parity port).
 *
 * PoP credits: strictly the 17 verification_evidence.py defs that had NO
 * pre-existing PoP annotation in src/ (21 defs are claimed by
 * verification_evidence.c / verif_evidence_wrappers.c / coding_context.c /
 * core_helpers.c and are NOT re-claimed here; they become universal seams).
 * Each claim appears ONCE, immediately above its `static ve2_*` definition;
 * this header paragraph is prose, not a claim.
 *
 * All flow kernels are file-local `static ve2_*` (the harness #includes this
 * file directly; no port .o / build registration).
 *
 * FLOW model: claimed siblings / stdlib collaborators are universal seams
 * driven by `in.io`. Every seam records {ev,args,ret} in call order; an
 * ABSENT script fail-closes with RuntimeError "no script for <name>". A
 * scripted {"__raise__":cls,"msg":...} becomes a pending exception. Callers
 * propagate it EXCEPT the `except ValueError` / `except (TypeError,ValueError)`
 * / `except Exception` sites, which use the FULL builtin exception hierarchy
 * below (BaseException trio passes through every except).
 *
 * DOMAIN: ASCII/UTF-8 fixture strings, integral ints, POSIX-shaped resolved
 * paths, small token lists; no exotic Unicode whitespace in strip positions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── small JSON helpers ────────────────────────────────────────────────── */
static json_t *ve2_null(void) { return json_null(); }
static json_t *ve2_jb(bool v) { return json_bool(v); }
static json_t *ve2_jn(long v) { return json_number((double)v); }
static json_t *ve2_js(const char *s) { return json_string(s ? s : ""); }
static json_t *ve2_obj(void) { return json_object(); }
static json_t *ve2_arr(void) { return json_array(); }
static json_t *ve2_pair(json_t *o, const char *k, json_t *v) { json_set(o, k, v); return o; }
static json_t *ve2_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }
static const char *ve2_s(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
static long ve2_n(const json_t *o, const char *k, long d) {
    const json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_NUMBER) ? (long)v->num_val : d;
}
static bool ve2_b(const json_t *o, const char *k, bool d) {
    const json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static bool ve2_haskey(const json_t *o, const char *k) { return json_obj_get(o, k) != NULL; }
static const char *ve2_sval(const json_t *v, const char *d) {
    return (v && v->type == JSON_STRING) ? v->str_val : d;
}
static long ve2_nval(const json_t *v, long d) {
    return (v && v->type == JSON_NUMBER) ? (long)v->num_val : d;
}
static bool ve2_strs_has(const json_t *a, const char *s) {
    if (!a || a->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(a); i++)
        if (strcmp(ve2_sval(json_get(a, i), ""), s) == 0) return true;
    return false;
}
static json_t *ve2_strs_json(const json_t *a) { return ve2_copy(a); }

/* Python truthiness for the JSON domain. */
static bool ve2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* ── universal seam + context ─────────────────────────────────────────── */
typedef struct ve2_ctx {
    const json_t *in;
    json_t *trace;
    json_t *seqpos;      /* per-seam __seq__ position */
    json_t *exc;         /* pending raise spec, or NULL */
    json_t *unattr;      /* state: unattributed-runs list */
    json_t *(*seam)(void *, const char *, const json_t *);
    void *seam_ud;
} ve2_ctx;

static bool ve2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static void ve2_raise(ve2_ctx *c, const char *cls, const char *msg) {
    if (!c) return;
    if (c->exc) json_free(c->exc);
    c->exc = json_object();
    json_set(c->exc, "__raise__", ve2_js(cls));
    json_set(c->exc, "msg", ve2_js(msg ? msg : ""));
}
static void ve2_exc_clear(ve2_ctx *c) {
    if (c && c->exc) { json_free(c->exc); c->exc = NULL; }
}
static bool ve2_pending(ve2_ctx *c) { return c && c->exc != NULL; }
static const char *ve2_exc_class(ve2_ctx *c) {
    return c && c->exc ? json_get_str(c->exc, "__raise__", "") : "";
}
/* seam call: records via ctx->seam, converts a raise ret into a pending exc. */
static json_t *ve2_call(ve2_ctx *c, const char *name, const json_t *args) {
    json_t *r = c->seam(c->seam_ud, name, args);
    if (ve2_is_raise(r)) {
        const char *cls = json_get_str(r, "__raise__", "Exception");
        const char *msg = json_get_str(r, "msg", "");
        ve2_raise(c, cls, msg);
        json_free(r);
        return NULL;
    }
    return r;   /* NULL pointer == scripted JSON null (Python None) */
}

/* FULL builtin exception hierarchy (parent map): except(Exception) matches
 * every Exception subclass but NOT the BaseException trio. */
static const char *ve2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"}, {"BaseException", NULL},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"}, {"JSONDecodeError", "ValueError"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"OSError", "Exception"}, {"FileNotFoundError", "OSError"},
        {"PermissionError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"FileExistsError", "OSError"},
        {"InterruptedError", "OSError"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"}, {"TimeoutError", "OSError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].c, c) == 0) return T[i].p;
    return NULL;
}
static bool ve2_exc_matches(ve2_ctx *c, const char *const *cls, size_t n) {
    if (!ve2_pending(c)) return false;
    const char *cur = ve2_exc_class(c);
    for (int depth = 0; depth < 64; depth++) {
        for (size_t i = 0; i < n; i++) if (strcmp(cur, cls[i]) == 0) return true;
        const char *p = ve2_exc_parent(cur);
        if (!p) return false;
        cur = p;
    }
    return false;
}
static bool ve2_exc_is_valueerror(ve2_ctx *c) {
    static const char *E[] = {"ValueError"};
    return ve2_exc_matches(c, E, 1);
}
static bool ve2_exc_is_te_or_ve(ve2_ctx *c) {
    static const char *E[] = {"TypeError", "ValueError"};
    return ve2_exc_matches(c, E, 2);
}
static bool ve2_exc_is_exception(ve2_ctx *c) {
    static const char *E[] = {"Exception"};
    return ve2_exc_matches(c, E, 1);
}

/* ── string helpers ───────────────────────────────────────────────────── */
static bool ve2_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || (c >= 0x1c && c <= 0x1f);
}
static char *ve2_strip(const char *s) {
    if (!s) return strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && ve2_ws((unsigned char)s[a])) a++;
    while (b > a && ve2_ws((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    memcpy(r, s + a, b - a); r[b - a] = '\0';
    return r;
}
/* re.sub(r"^(?:\./)+", "", s) */
static const char *ve2_no_dotslash(const char *s) {
    while (s && s[0] == '.' && s[1] == '/') s += 2;
    return s ? s : "";
}
/* token.replace("\\","/").rsplit("/",1)[-1] */
static const char *ve2_basename(const char *tok) {
    static char buf[2048];
    snprintf(buf, sizeof(buf), "%s", tok ? tok : "");
    for (char *p = buf; *p; p++) if (*p == '\\') *p = '/';
    char *slash = strrchr(buf, '/');
    return slash ? slash + 1 : buf;
}
/* str.endswith one of .exe/.bat/.cmd, case-insensitive (one suffix) */
static bool ve2_exe_suffix(const char *s) {
    size_t n = strlen(s);
    if (n < 4 || s[n - 4] != '.') return false;
    const char *e = s + n - 3;
    return strcasecmp(e, "exe") == 0 || strcasecmp(e, "bat") == 0 || strcasecmp(e, "cmd") == 0;
}
/* suffix helper for _INTERPRETER_NAME_RE: [0-9]+(\.[0-9]+)* or empty */
static bool ve2_interp_suffix(const char *r) {
    if (*r == '\0') return true;
    if (!isdigit((unsigned char)*r)) return false;
    while (*r && isdigit((unsigned char)*r)) r++;
    while (*r == '.') {
        r++;
        if (!isdigit((unsigned char)*r)) return false;
        while (*r && isdigit((unsigned char)*r)) r++;
    }
    return *r == '\0';
}
/* _INTERPRETER_NAME_RE -- try every prefix (the regex backtracks from the
 * longest name to a shorter one when the suffix does not match, e.g.
 * python3.12 matches `python` + `3.12`, not `python3` + `.12`). */
static bool ve2_interp_name(const char *s) {
    static const char *NAMES[] = {"python3", "python", "perl", "ruby", "bash", "node", "py", "sh"};
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        size_t k = strlen(NAMES[i]);
        if (strncmp(s, NAMES[i], k) == 0 && ve2_interp_suffix(s + k)) return true;
    }
    return false;
}
/* re.sub(r"[0-9.]+$", "", s) -> family */
static const char *ve2_family_from_name(const char *s) {
    static char buf[128];
    size_t n = strlen(s);
    while (n > 0 && (isdigit((unsigned char)s[n - 1]) || s[n - 1] == '.')) n--;
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n); buf[n] = '\0';
    return buf;
}
/* _TIMEOUT_DURATION_RE: ^\d+(?:\.\d+)?[smhd]?$ */
static bool ve2_timeout_duration(const char *s) {
    if (!s || !isdigit((unsigned char)*s)) return false;
    while (*s && isdigit((unsigned char)*s)) s++;
    if (*s == '.') {
        s++;
        if (!isdigit((unsigned char)*s)) return false;
        while (*s && isdigit((unsigned char)*s)) s++;
    }
    if (*s && strchr("smhd", *s)) s++;
    return *s == '\0';
}
/* _INTERPRETER_FLAG_RE: ^-(?:[IBuEsSOd]+|3(\.\d+)?|[WX].*)$ */
static bool ve2_harmless_flag(const char *s) {
    if (!s || s[0] != '-') return false;
    const char *p = s + 1;
    if (*p == '\0') return false;
    if (p[0] == 'W' || p[0] == 'X') return true;     /* [WX].* */
    if (p[0] == '3') {
        p++;
        if (*p == '\0') return true;
        if (*p != '.') return false;
        p++;
        if (!isdigit((unsigned char)*p)) return false;
        while (*p && isdigit((unsigned char)*p)) p++;
        return *p == '\0';
    }
    for (const char *q = p; *q; q++)
        if (!strchr("IBuEsSOd", *q)) return false;
    return true;
}

/* path ancestor: resolved == base OR base in resolved.parents (POSIX) */
static bool ve2_under_path(const char *resolved, const char *base) {
    if (!resolved || !base) return false;
    if (strcmp(resolved, base) == 0) return true;
    size_t rl = strlen(resolved), bl = strlen(base);
    if (bl >= rl) return false;
    if (strncmp(resolved, base, bl) != 0) return false;
    return resolved[bl] == '/' || base[bl - 1] == '/';
}

/* ── seam arg builders ────────────────────────────────────────────────── */
static json_t *ve2_args(void) { return json_object(); }
static json_t *ve2_arg(json_t *o, const char *k, json_t *v) { json_set(o, k, v); return o; }
static json_t *ve2_args1(const char *k, json_t *v) { return ve2_arg(json_object(), k, v); }

/* ── ported-sibling call helpers (compose inline, seam claimed siblings) ── */
static bool ve2_is_under(ve2_ctx *, const char *, const char *);
static bool ve2_is_temp_script_path(ve2_ctx *, const char *, const char *);
static bool ve2_is_interpreter_token(const char *);
static char *ve2_root_for(ve2_ctx *, const json_t *, const char *);
static char *ve2_str_of(const json_t *v);

/* ========================================================================
 * 17 PORTED DEFS -- one PoP annotation immediately above each definition.
 * ======================================================================== */

/* PoP: ve2_ledger_enabled @ agent/verification_evidence.py:_ledger_enabled */
static bool ve2_ledger_enabled(ve2_ctx *c) {
    json_t *h = ve2_call(c, "host_feature_enabled",
                         ve2_args1("feature", ve2_js("tool_host_artifacts")));
    if (ve2_pending(c)) return false;              /* no except: propagate */
    bool first = ve2_truthy(h);
    if (h) json_free(h);
    if (!first) return false;                      /* `and` short-circuit */
    json_t *v = ve2_call(c, "verify_on_stop_enabled", ve2_args());
    if (ve2_pending(c)) return false;
    bool r = ve2_truthy(v);
    if (v) json_free(v);
    return r;
}

/* stable operator literals (no shared scratch buffer) */
static const char *ve2_op_lit(char ch, char next, size_t olen) {
    if (olen == 2) return ch == '&' ? "&&" : (next == '|' ? "||" : "|&");
    if (ch == '\n' || ch == ';') return ";";
    if (ch == '|') return "|";
    return "&";
}

/* PoP: ve2_split_shell_segments @ agent/verification_evidence.py:_split_shell_segments */
static json_t *ve2_split_shell_segments(ve2_ctx *c, const char *command, bool posix) {
    json_t *out = ve2_arr();
    if (!command) return out;
    const char *raw[512]; const char *op[512]; size_t nraw = 0;
    size_t start = 0, index = 0;
    char quote = 0; bool escaped = false;
    size_t len = strlen(command);
    while (index < len) {
        char ch = command[index];
        if (escaped || (ch == '\\' && quote != '\'')) {
            escaped = !escaped; index++; continue;
        }
        if (quote || ch == '\'' || ch == '"') {
            if (ch == quote) quote = 0;
            else if (!quote) quote = ch;
            index++; continue;
        }
        const char *oper = NULL; size_t olen = 0;
        if ((ch == '&' && command[index + 1] == '&') ||
            (ch == '|' && (command[index + 1] == '|' || command[index + 1] == '&'))) {
            oper = command + index; olen = 2;
        } else if (ch == '\n') {
            oper = ";"; olen = 1;
        } else if (ch == ';' || ch == '|' ||
                   (ch == '&' && (index == 0 || (command[index - 1] != '<' && command[index - 1] != '>')) &&
                    !(command[index + 1] == '>'))) {
            oper = command + index; olen = 1;
        }
        if (!oper) { index++; continue; }
        char rawbuf[2048];
        size_t n = index - start;
        if (n >= sizeof(rawbuf)) n = sizeof(rawbuf) - 1;
        memcpy(rawbuf, command + start, n); rawbuf[n] = '\0';
        char *trim = ve2_strip(rawbuf);
        if (!*trim) { free(trim); json_free(out); return ve2_arr(); }
        if (nraw < 512) {
            raw[nraw] = strdup(trim);
            op[nraw] = ve2_op_lit(ch, command[index + 1], olen);
            nraw++;
        }
        free(trim);
        index += (ch == '\n') ? 1 : olen;
        start = index;
    }
    if (quote || escaped) {
        for (size_t i = 0; i < nraw; i++) free((void *)raw[i]);
        json_free(out); return ve2_arr();
    }
    char *tr = ve2_strip(command + start);
    if (*tr) {
        if (nraw < 512) { raw[nraw] = strdup(tr); op[nraw] = NULL; nraw++; }
    } else if (nraw && op[nraw - 1] != NULL && strcmp(op[nraw - 1], ";") != 0) {
        for (size_t i = 0; i < nraw; i++) free((void *)raw[i]);
        free(tr); json_free(out); return ve2_arr();
    }
    free(tr);
    json_t *segs = ve2_arr();
    for (size_t i = 0; i < nraw; i++) {
        json_t *a = ve2_arg(ve2_args(), "raw", ve2_js(raw[i]));
        json_set(a, "posix", ve2_jb(posix));
        json_t *tv = ve2_call(c, "shlex.split", a);
        if (ve2_pending(c)) {
            if (ve2_exc_is_valueerror(c)) { ve2_exc_clear(c); /* except ValueError -> [] */
                for (size_t k = 0; k < nraw; k++) free((void *)raw[k]);
                json_free(segs); json_free(out); return ve2_arr();
            }
            for (size_t k = 0; k < nraw; k++) free((void *)raw[k]);
            json_free(segs); json_free(out); return NULL;  /* propagate */
        }
        bool empty = !tv || tv->type != JSON_ARRAY || json_len(tv) == 0;
        json_t *e = ve2_obj();
        json_set(e, "tokens", empty ? ve2_arr() : ve2_copy(tv));
        json_set(e, "following_operator", op[i] ? ve2_js(op[i]) : ve2_null());
        json_append(segs, e);
        if (tv) json_free(tv);
        if (empty) {
            for (size_t k = 0; k < nraw; k++) free((void *)raw[k]);
            json_free(segs); json_free(out); return ve2_arr();
        }
    }
    for (size_t i = 0; i < nraw; i++) free((void *)raw[i]);
    json_free(out);
    return segs;
}

/* PoP: ve2_segment_status @ agent/verification_evidence.py:_segment_status */
static json_t *ve2_segment_status(const json_t *segments, long match_index,
                                  long exit_code, const json_t *pipeline) {
    size_t ns = (segments && segments->type == JSON_ARRAY) ? json_len(segments) : 0;
    if (ns == 0 || match_index < 0 || match_index >= (long)ns)
        return ve2_null();
    for (size_t i = 0; i < ns; i++) {
        const json_t *s = json_get(segments, i);
        if (strcmp(ve2_sval(json_obj_get(s, "following_operator"), ""), "&") == 0)
            return ve2_null();
    }
    /* sequence_start = max(i+1 for i,s in enumerate(segments[:-1]) if op==';') */
    long seq_start = 0;
    for (size_t i = 0; i + 1 < ns; i++) {
        const json_t *s = json_get(segments, i);
        const char *o = ve2_sval(json_obj_get(s, "following_operator"), "");
        if (strcmp(o, ";") == 0 && (long)(i + 1) > seq_start) seq_start = (long)(i + 1);
    }
    if (match_index < seq_start) return ve2_null();
    /* operators = set(op for s in segments[sequence_start:-1]) */
    bool has_or = false, has_pipe = false, has_and = false, has_other = false;
    for (size_t i = (size_t)seq_start; i + 1 < ns; i++) {
        const json_t *s = json_get(segments, i);
        const char *o = ve2_sval(json_obj_get(s, "following_operator"), "");
        if (!*o) continue;
        if (strcmp(o, "||") == 0) has_or = true;
        else if (strcmp(o, "|") == 0 || strcmp(o, "|&") == 0) has_pipe = true;
        else if (strcmp(o, "&&") == 0) has_and = true;
        else has_other = true;
    }
    if (has_or) return ve2_null();
    if (has_pipe) {
        long pipe_start = seq_start;
        for (size_t i = (size_t)seq_start; i + 1 < ns; i++) {
            const json_t *s = json_get(segments, i);
            const char *o = ve2_sval(json_obj_get(s, "following_operator"), "");
            if (strcmp(o, "|") != 0 && strcmp(o, "|&") != 0) {
                long cand = seq_start + (long)i + 1;
                if (cand > pipe_start) pipe_start = cand;
            }
        }
        bool chain_ran = !has_and || exit_code == 0;
        if (chain_ran && pipeline && pipeline->type == JSON_ARRAY && json_len(pipeline) >= 2 &&
            match_index == pipe_start)
            return ve2_copy(json_get(pipeline, 0));
        return ve2_null();
    }
    if (!has_or && !has_pipe && !has_and && !has_other) return ve2_jn(exit_code);
    if (has_and && !has_other) return exit_code == 0 ? ve2_jn(exit_code) : ve2_null();
    return ve2_null();
}

/* PoP: ve2_wrapper_option_width @ agent/verification_evidence.py:_wrapper_option_width */
static long ve2_wrapper_option_width(const char *tok, const json_t *flags, const json_t *valued) {
    if (ve2_strs_has(flags, tok)) return 1;
    if (ve2_strs_has(valued, tok)) return 2;
    if (valued && valued->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(valued); i++) {
            const char *v = ve2_sval(json_get(valued, i), "");
            if (!*v) continue;
            if (v[0] == '-' && v[1] == '-') {
                if (strncmp(tok, v, strlen(v)) == 0 && tok[strlen(v)] == '=') return 1;
            } else if (strncmp(tok, v, strlen(v)) == 0) {
                return 1;
            }
        }
    return 0;
}

/* PoP: ve2_skip_wrapper @ agent/verification_evidence.py:_skip_wrapper */
static json_t *ve2_skip_wrapper(ve2_ctx *c, const json_t *tokens, long ii) {
    size_t n = (tokens && tokens->type == JSON_ARRAY) ? json_len(tokens) : 0;
    if (ii < 0 || (size_t)ii >= n) return ve2_null();
    const char *w = ve2_sval(json_get(tokens, (size_t)ii), "");
    bool timeout = strcmp(w, "timeout") == 0;
    const char *flags = NULL; const char *valued = NULL;
    if (timeout) { flags = "-p --preserve-status -f --foreground -v --verbose"; valued = "-s --signal -k --kill-after"; }
    else if (strcmp(w, "nice") == 0) { flags = ""; valued = "-n --adjustment"; }
    else if (strcmp(w, "nohup") == 0) { flags = ""; valued = ""; }
    else if (strcmp(w, "stdbuf") == 0) { flags = ""; valued = "-i -o -e --input --output --error"; }
    else {
        /* _WRAPPER_OPTIONS[tokens[i]] -- uncaught KeyError (no try site) */
        char kb[256];
        snprintf(kb, sizeof(kb), "'%s'", w);
        ve2_raise(c, "KeyError", kb);
        return NULL;
    }
    /* build temp JSON arrays from space-separated lists */
    json_t *fj = ve2_arr(), *vj = ve2_arr();
    char fbuf[512], vbuf[512];
    snprintf(fbuf, sizeof(fbuf), "%s", flags); snprintf(vbuf, sizeof(vbuf), "%s", valued);
    char *p;
    for (p = strtok(fbuf, " "); p; p = strtok(NULL, " ")) json_append(fj, ve2_js(p));
    for (p = strtok(vbuf, " "); p; p = strtok(NULL, " ")) json_append(vj, ve2_js(p));
    long i = ii + 1;
    while (i < (long)n) {
        const char *t = ve2_sval(json_get(tokens, (size_t)i), "");
        if (t[0] != '-' || strcmp(t, "--") == 0) break;
        long width = ve2_wrapper_option_width(t, fj, vj);
        if (!width) { json_free(fj); json_free(vj); return ve2_null(); }
        i += width;
    }
    if (i < (long)n && strcmp(ve2_sval(json_get(tokens, (size_t)i), ""), "--") == 0) i++;
    if (timeout) {
        if (i >= (long)n || !ve2_timeout_duration(ve2_sval(json_get(tokens, (size_t)i), ""))) {
            json_free(fj); json_free(vj); return ve2_null();
        }
        i++;
    }
    json_free(fj); json_free(vj);
    return ve2_jn(i);
}

/* PoP: ve2_interpreter_family @ agent/verification_evidence.py:_interpreter_family */
static json_t *ve2_interpreter_family(const char *tok) {
    const char *name = ve2_basename(tok);
    name = ve2_no_dotslash(name);
    static char clean[2048];
    if (ve2_exe_suffix(name)) {
        size_t k = strlen(name) - 4;
        if (k >= sizeof(clean)) k = sizeof(clean) - 1;
        memcpy(clean, name, k); clean[k] = '\0';
    } else snprintf(clean, sizeof(clean), "%s", name);
    if (!ve2_interp_name(clean)) return ve2_null();
    const char *fam = ve2_family_from_name(clean);
    return ve2_js(strcmp(fam, "py") == 0 ? "python" : fam);
}

/* PoP: ve2_is_harmless_interpreter_flag @ agent/verification_evidence.py:_is_harmless_interpreter_flag */
static bool ve2_is_harmless_interpreter_flag(const char *tok) {
    return ve2_harmless_flag(tok);
}

/* PoP: ve2_normalize_tokens @ agent/verification_evidence.py:_normalize_tokens */
static json_t *ve2_normalize_tokens(const json_t *tokens) {
    json_t *out = ve2_arr();
    size_t n = (tokens && tokens->type == JSON_ARRAY) ? json_len(tokens) : 0;
    size_t i = 0;
    while (i < n) {
        const char *t = ve2_sval(json_get(tokens, i), "");
        json_t *fam = ve2_interpreter_family(t);
        const char *f = (fam && fam->type == JSON_STRING) ? fam->str_val : NULL;
        if (f) json_append(out, ve2_js(f));
        else json_append(out, ve2_js(ve2_no_dotslash(t)));
        if (fam) json_free(fam);
        i++;
        if (f && strcmp(f, "python") == 0) {
            while (i < n) {
                const char *fl = ve2_sval(json_get(tokens, i), "");
                if (!ve2_harmless_flag(fl)) break;
                i += (strcmp(fl, "-W") == 0 || strcmp(fl, "-X") == 0) ? 2 : 1;
            }
        }
    }
    return out;
}

/* PoP: ve2_is_under @ agent/verification_evidence.py:_is_under */
static bool ve2_is_under(ve2_ctx *c, const char *token, const char *base) {
    if (!base || !*base) return false;
    json_t *a = ve2_call(c, "path.is_absolute", ve2_args1("path", ve2_js(token)));
    if (ve2_pending(c)) {
        bool caught = ve2_exc_is_exception(c);
        if (caught) ve2_exc_clear(c);
        if (a) json_free(a);
        return false;                       /* except Exception -> False; Base -> propagate flag */
    }
    bool abs = ve2_truthy(a);
    if (a) json_free(a);
    if (!abs) return false;
    json_t *t = ve2_call(c, "path.resolve", ve2_args1("path", ve2_js(token)));
    if (ve2_pending(c)) { bool caught = ve2_exc_is_exception(c); if (caught) ve2_exc_clear(c); return false; }
    json_t *b = ve2_call(c, "path.resolve", ve2_args1("path", ve2_js(base)));
    if (ve2_pending(c)) { bool caught = ve2_exc_is_exception(c); if (caught) ve2_exc_clear(c); if (t) json_free(t); return false; }
    bool r = ve2_under_path(ve2_sval(t, ""), ve2_sval(b, ""));
    if (t) json_free(t);
    if (b) json_free(b);
    return r;
}

/* claimed sibling (verification_evidence.c) -- re-derived internal helper,
 * NO PoP claim: composed inline by ve2_ad_hoc_script_args for the standalone
 * differential check. */
static bool ve2_is_temp_script_path(ve2_ctx *c, const char *token, const char *root) {
    json_t *nm = ve2_call(c, "path.name", ve2_args1("path", ve2_js(token)));
    if (ve2_pending(c)) {
        bool caught = ve2_exc_is_exception(c);
        if (caught) ve2_exc_clear(c);
        if (nm) json_free(nm);
        return false;
    }
    const char *name = ve2_sval(nm, "");
    bool pref = strncmp(name, "hermes-verify-", 14) == 0 || strncmp(name, "hermes-ad-hoc-", 14) == 0;
    if (nm) json_free(nm);
    if (!pref) return false;
    json_t *td = ve2_call(c, "tempfile.gettempdir", ve2_args());
    if (ve2_pending(c)) return false;
    const char *tmp = ve2_sval(td, "");
    bool u1 = ve2_is_under(c, token, tmp);
    if (ve2_pending(c)) { if (td) json_free(td); return false; }
    bool r = false;
    if (u1) {
        bool u2 = ve2_is_under(c, token, root);
        r = !u2;
    }
    if (td) json_free(td);
    return r && !ve2_pending(c);
}

/* PoP: ve2_is_interpreter_token @ agent/verification_evidence.py:_is_interpreter_token */
static bool ve2_is_interpreter_token(const char *tok) {
    const char *name = ve2_basename(tok);
    static char clean[2048];
    if (ve2_exe_suffix(name)) {
        size_t k = strlen(name) - 4;
        if (k >= sizeof(clean)) k = sizeof(clean) - 1;
        memcpy(clean, name, k); clean[k] = '\0';
    } else snprintf(clean, sizeof(clean), "%s", name);
    return ve2_interp_name(clean);
}

/* claimed sibling (verification_evidence.c / verif_evidence_wrappers.c) --
 * re-derived internal helper, NO PoP claim: composed inline and driven as an
 * extra differential-check target. */
static json_t *ve2_ad_hoc_script_args(ve2_ctx *c, const json_t *tokens, const char *root) {
    json_t *ca = ve2_call(c, "strip_command_prefix",
                          ve2_args1("tokens", ve2_copy(tokens)));
    if (ve2_pending(c)) return ve2_null();
    size_t n = (ca && ca->type == JSON_ARRAY) ? json_len(ca) : 0;
    if (n == 0) { if (ca) json_free(ca); return ve2_null(); }
    const char *cmd = ve2_sval(json_get(ca, 0), "");
    if (ve2_is_temp_script_path(c, cmd, root)) {
        json_t *r = ve2_arr();
        for (size_t i = 1; i < n; i++) json_append(r, ve2_copy(json_get(ca, i)));
        json_free(ca);
        return r;
    }
    json_t *nm = ve2_call(c, "path.name", ve2_args1("path", ve2_js(cmd)));
    if (ve2_pending(c)) { json_free(ca); return ve2_null(); }
    bool is_env = strcmp(ve2_sval(nm, ""), "env") == 0 && n > 1 &&
                  ve2_is_interpreter_token(ve2_sval(json_get(ca, 1), ""));
    if (nm) json_free(nm);
    size_t base = 0;
    if (is_env) base = 1;
    const char *command = ve2_sval(json_get(ca, base), "");
    if (ve2_is_interpreter_token(command)) {
        for (size_t idx = 1; idx < n; idx++) {
            const char *t = ve2_sval(json_get(ca, idx), "");
            if (ve2_is_temp_script_path(c, t, root)) {
                json_t *r = ve2_arr();
                for (size_t j = idx + 1; j < n; j++) json_append(r, ve2_copy(json_get(ca, j)));
                json_free(ca);
                return r;
            }
            if (strcmp(t, "--") != 0 && t[0] != '-') { json_free(ca); return ve2_null(); }
        }
    }
    (void)base;
    json_free(ca);
    return ve2_null();
}

/* PoP: ve2_root_for @ agent/verification_evidence.py:_root_for */
static char *ve2_root_for(ve2_ctx *c, const json_t *facts, const char *cwd) {
    const char *r = NULL;
    if (facts && facts->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(facts, "root");
        if (v && ve2_truthy(v)) r = ve2_sval(v, "");
    }
    if (r) return strdup(r);
    json_t *p = ve2_call(c, "path.resolve", ve2_args1("path", ve2_js(cwd && *cwd ? cwd : ".")));
    if (ve2_pending(c)) return strdup("");
    char *s = strdup(ve2_sval(p, ""));
    if (p) json_free(p);
    return s;
}

/* PoP: ve2_load_changed_paths @ agent/verification_evidence.py:_load_changed_paths */
static json_t *ve2_load_changed_paths(ve2_ctx *c, const char *raw) {
    const char *s = (raw && *raw) ? raw : "[]";
    json_t *v = ve2_call(c, "json.loads", ve2_args1("s", ve2_js(s)));
    if (ve2_pending(c)) {
        if (ve2_exc_is_te_or_ve(c)) { ve2_exc_clear(c); return ve2_arr(); }
        return NULL;                         /* propagate */
    }
    return v ? v : ve2_arr();
}

/* PoP: ve2_classify_in_workspace @ agent/verification_evidence.py:_classify_in_workspace */
static json_t *ve2_classify_in_workspace(ve2_ctx *c, const char *command,
                                          const json_t *facts, const char *cwd,
                                          const char *session_id, long exit_code,
                                          const char *output, const json_t *pipeline) {
    json_t *vc = (facts && facts->type == JSON_OBJECT) ? json_obj_get(facts, "verifyCommands") : NULL;
    json_t *cmds = (vc && vc->type == JSON_ARRAY) ? ve2_copy(vc) : ve2_arr();
    json_t *a = ve2_obj();
    json_set(a, "command", ve2_js(command));
    json_set(a, "canonical_commands", cmds);
    json_set(a, "exit_code", ve2_jn(exit_code));
    json_set(a, "pipeline_status", pipeline ? ve2_copy(pipeline) : ve2_null());
    json_t *m = ve2_call(c, "find_canonical_match", a);
    if (ve2_pending(c)) { if (m) json_free(m); return ve2_null(); }
    bool matched = m && m->type == JSON_ARRAY && json_len(m) >= 3 &&
                   json_get(m, 2) && json_get(m, 2)->type != JSON_NULL;
    const char *canonical = "ad-hoc verification script";
    const char *kind = "ad_hoc";
    const char *scope = "targeted";
    long status = exit_code;
    if (matched) {
        static char canonbuf[1024];
        snprintf(canonbuf, sizeof(canonbuf), "%s", ve2_sval(json_get(m, 0), ""));
        canonical = canonbuf;
        const json_t *trail = json_get(m, 1);
        json_t *ka = ve2_call(c, "kind_for_command", ve2_args1("canonical", ve2_js(canonical)));
        if (ve2_pending(c)) { if (m) json_free(m); return ve2_null(); }
        kind = strdup(ve2_sval(ka, ""));
        if (ka) json_free(ka);
        bool targeted = false;
        size_t nt = (trail && trail->type == JSON_ARRAY) ? json_len(trail) : 0;
        for (size_t i = 0; i < nt; i++) {
            json_t *la = ve2_args1("arg", ve2_copy(json_get(trail, i)));
            json_t *lv = ve2_call(c, "looks_like_target", la);
            if (ve2_pending(c)) break;
            if (ve2_truthy(lv)) { targeted = true; if (lv) json_free(lv); break; }
            if (lv) json_free(lv);
        }
        scope = targeted ? "targeted" : "full";
        status = ve2_nval(json_get(m, 2), exit_code);
    } else {
        json_t *fa = ve2_obj();
        json_set(fa, "command", ve2_js(command));
        json_set(fa, "root", (facts && json_obj_get(facts, "root")) ? ve2_copy(json_obj_get(facts, "root")) : ve2_null());
        json_set(fa, "exit_code", ve2_jn(exit_code));
        /* Python: `if verify_commands or _find_ad_hoc_match(...) is None` --
         * the seam is NOT called when verify_commands is non-empty. */
        if (json_len(cmds) > 0) {
            json_free(fa);
            if (m) json_free(m);
            return ve2_null();
        }
        json_t *adh = ve2_call(c, "find_ad_hoc_match", fa);
        json_free(fa);
        if (ve2_pending(c)) { if (adh) json_free(adh); if (m) json_free(m); return ve2_null(); }
        if (!ve2_truthy(adh)) {
            if (adh) json_free(adh);
            if (m) json_free(m);
            return ve2_null();
        }
        if (adh) json_free(adh);
        status = exit_code;
    }
    if (m) json_free(m);
    json_t *pa = ve2_call(c, "path.resolve", ve2_args1("path", ve2_js(cwd && *cwd ? cwd : ".")));
    if (ve2_pending(c)) { if (pa) json_free(pa); return ve2_null(); }
    char *cwd_res = strdup(ve2_sval(pa, ""));
    if (pa) json_free(pa);
    char *root = ve2_root_for(c, facts, cwd);
    json_t *sa = ve2_call(c, "summarize_output", ve2_args1("output", ve2_js(output)));
    if (ve2_pending(c)) { if (sa) json_free(sa); free(root); free(cwd_res); return ve2_null(); }
    const char *sum = ve2_sval(sa, "");
    json_t *ev = ve2_obj();
    json_set(ev, "command", ve2_js(command));
    json_set(ev, "canonical_command", ve2_js(canonical));
    json_set(ev, "kind", ve2_js(kind));
    json_set(ev, "scope", ve2_js(scope));
    json_set(ev, "status", ve2_js(status == 0 ? "passed" : "failed"));
    json_set(ev, "exit_code", ve2_jn(exit_code));
    json_set(ev, "cwd", ve2_js(cwd_res));
    json_set(ev, "root", ve2_js(root));
    json_set(ev, "session_id", ve2_js(session_id && *session_id ? session_id : "default"));
    json_set(ev, "output_summary", ve2_js(sum));
    if (sa) json_free(sa);
    free(root); free(cwd_res);
    return ev;
}

/* PoP: ve2_last_unattributed_run @ agent/verification_evidence.py:last_unattributed_run */
static json_t *ve2_last_unattributed_run(const char *session_id, const char *root,
                                         const char *since, const json_t *unattr) {
    const char *sid = (session_id && *session_id) ? session_id : "default";
    const char *found = NULL;
    if (unattr && unattr->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(unattr); i++) {
            const json_t *r = json_get(unattr, i);
            if (strcmp(ve2_sval(json_obj_get(r, "session_id"), ""), sid) == 0 &&
                strcmp(ve2_sval(json_obj_get(r, "root"), ""), (root ? root : "")) == 0) {
                found = ve2_sval(json_obj_get(r, "command"), "");
                const char *at = ve2_sval(json_obj_get(r, "at"), "");
                if (since && *since && strcmp(since, at) >= 0) return ve2_null();
                break;
            }
        }
    return found ? ve2_js(found) : ve2_null();
}

/* ── insert_evidence / newest_workspace_verify (compose claimed seams) ── */
static json_t *ve2_insert_evidence(ve2_ctx *c, const json_t *ev);

/* PoP: ve2_record_verify_run @ agent/verification_evidence.py:record_verify_run */
static json_t *ve2_record_verify_run(ve2_ctx *c, const char *root, const char *session_id,
                                      bool ok, const char *command, const char *scope,
                                      const char *output) {
    if (!ve2_ledger_enabled(c)) {
        if (ve2_pending(c)) return NULL;
        return ve2_null();
    }
    json_t *pa = ve2_call(c, "path.resolve", ve2_args1("path", ve2_js(root ? root : ".")));
    if (ve2_pending(c)) { if (pa) json_free(pa); return NULL; }
    char *resolved = strdup(ve2_sval(pa, ""));
    if (pa) json_free(pa);
    json_t *fa = ve2_call(c, "project_facts", ve2_args1("root", ve2_js(root ? root : ".")));
    if (ve2_pending(c)) { if (fa) json_free(fa); free(resolved); return NULL; }
    char *eroot = NULL;
    const json_t *fr = (fa && fa->type == JSON_OBJECT) ? json_obj_get(fa, "root") : NULL;
    if (fr && ve2_truthy(fr)) eroot = strdup(ve2_sval(fr, ""));
    else eroot = strdup(resolved);
    if (fa) json_free(fa);
    json_t *sa = ve2_call(c, "summarize_output", ve2_args1("output", ve2_js(output)));
    if (ve2_pending(c)) { if (sa) json_free(sa); free(eroot); free(resolved); return NULL; }
    const char *sum = ve2_sval(sa, "");
    json_t *ev = ve2_obj();
    json_set(ev, "command", ve2_js(command));
    json_set(ev, "canonical_command", ve2_js("hermes verify"));
    json_set(ev, "kind", ve2_js("verify"));
    json_set(ev, "scope", ve2_js(scope && (strcmp(scope, "full") == 0 || strcmp(scope, "targeted") == 0) ? scope : "full"));
    json_set(ev, "status", ve2_js(ok ? "passed" : "failed"));
    json_set(ev, "exit_code", ve2_jn(ok ? 0 : 1));
    json_set(ev, "cwd", ve2_js(resolved));
    json_set(ev, "root", ve2_js(eroot));
    json_set(ev, "session_id", ve2_js(session_id && *session_id ? session_id : "default"));
    json_set(ev, "output_summary", ve2_js(sum));
    if (sa) json_free(sa);
    free(eroot); free(resolved);
    json_t *r = ve2_insert_evidence(c, ev);
    json_free(ev);
    return r;
}

/* DB execute seam: returns a json value (row or {"lastrowid":N}) or raise spec. */
static json_t *ve2_dbexec(ve2_ctx *c, const char *tag, const json_t *params) {
    json_t *a = ve2_obj();
    json_set(a, "sql", ve2_js(tag));
    json_set(a, "params", ve2_copy(params));
    return ve2_call(c, "db.execute", a);
}

/* PoP: ve2_insert_evidence @ agent/verification_evidence.py:_insert_evidence */
static json_t *ve2_insert_evidence(ve2_ctx *c, const json_t *ev) {
    json_t *ua = ve2_call(c, "utc_now", ve2_args());
    if (ve2_pending(c)) { if (ua) json_free(ua); return NULL; }
    const char *created = ve2_sval(ua, "");
    const char *sid = ve2_sval(json_obj_get(ev, "session_id"), "default");
    const char *root = ve2_sval(json_obj_get(ev, "root"), "");
    /* _UNATTRIBUTED_RUNS.pop((sid,root), None) */
    if (c->unattr && c->unattr->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(c->unattr); i++) {
            const json_t *r = json_get(c->unattr, i);
            if (strcmp(ve2_sval(json_obj_get(r, "session_id"), ""), sid) == 0 &&
                strcmp(ve2_sval(json_obj_get(r, "root"), ""), root) == 0) {
                json_array_remove(c->unattr, i);
                break;
            }
        }
    json_t *ta = ve2_call(c, "transaction", ve2_args());
    if (ve2_pending(c)) { if (ta) json_free(ta); if (ua) json_free(ua); return NULL; }
    if (ta) json_free(ta);
    /* first INSERT -> lastrowid; params follow the live SQL tuple order */
    json_t *pa = ve2_arr();
    json_append(pa, ve2_js(created));
    json_append(pa, ve2_js(sid));
    const char *keys[] = {"cwd", "root", "command", "canonical_command",
                          "kind", "scope", "status"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        json_append(pa, ve2_copy(json_obj_get(ev, keys[i])));
    json_append(pa, ve2_copy(json_obj_get(ev, "exit_code")));
    json_append(pa, ve2_copy(json_obj_get(ev, "output_summary")));
    json_t *cur = ve2_dbexec(c, "insert_event", pa);
    json_free(pa);
    if (ve2_pending(c)) { if (cur) json_free(cur); if (ua) json_free(ua); return NULL; }
    long event_id = (cur && cur->type == JSON_OBJECT && json_obj_get(cur, "lastrowid"))
                    ? ve2_nval(json_obj_get(cur, "lastrowid"), -1) : -1;
    if (cur) json_free(cur);
    if (event_id < 0) {
        ve2_raise(c, "RuntimeError", "verification event insert did not return an id");
        if (ua) json_free(ua);
        return NULL;
    }
    json_t *pa2 = ve2_arr();
    json_append(pa2, ve2_js(sid));
    json_append(pa2, ve2_js(root));
    json_append(pa2, ve2_jn(event_id));
    json_t *e2 = ve2_dbexec(c, "upsert_state", pa2);
    if (ve2_pending(c)) { if (e2) json_free(e2); if (ua) json_free(ua); return NULL; }
    if (e2) json_free(e2);
    const char *kind = ve2_sval(json_obj_get(ev, "kind"), "");
    const char *status = ve2_sval(json_obj_get(ev, "status"), "");
    const char *scope = ve2_sval(json_obj_get(ev, "scope"), "");
    if (strcmp(kind, "verify") == 0 && strcmp(status, "passed") == 0 && strcmp(scope, "full") == 0) {
        json_t *pa3 = ve2_arr();
        json_append(pa3, ve2_jn(event_id));
        json_append(pa3, ve2_js(root));
        json_append(pa3, ve2_js(sid));
        json_t *e3 = ve2_dbexec(c, "update_other_sessions", pa3);
        if (ve2_pending(c)) { if (e3) json_free(e3); if (ua) json_free(ua); return NULL; }
        if (e3) json_free(e3);
    }
    json_t *pa4 = ve2_obj();
    json_set(pa4, "session_id", ve2_js(sid));
    json_set(pa4, "root", ve2_js(root));
    json_t *e4 = ve2_call(c, "prune_old_events", pa4);
    if (ve2_pending(c)) { if (e4) json_free(e4); if (ua) json_free(ua); return NULL; }
    if (e4) json_free(e4);
    json_t *cm = ve2_call(c, "db.commit", ve2_args());
    if (ve2_pending(c)) { if (cm) json_free(cm); if (ua) json_free(ua); return NULL; }
    if (cm) json_free(cm);
    /* return {"id": id, **e.__dict__, "created_at": created} */
    json_t *r = ve2_copy(ev);
    json_set(r, "id", ve2_jn(event_id));
    json_set(r, "created_at", ve2_js(created));
    if (ua) json_free(ua);
    return r;
}

/* PoP: ve2_newest_workspace_verify @ agent/verification_evidence.py:_newest_workspace_verify */
static json_t *ve2_newest_workspace_verify(ve2_ctx *c, const char *root, const char *since) {
    json_t *pa = ve2_arr();
    json_append(pa, ve2_js(root ? root : ""));
    if (since && *since) json_append(pa, ve2_js(since));
    json_t *row = ve2_dbexec(c, since && *since ? "select_newest_since" : "select_newest", pa);
    json_free(pa);
    if (ve2_pending(c)) { if (row) json_free(row); return NULL; }
    return row ? row : ve2_null();
}

/* helper: JSON -> freshly allocated C string (for state echo where needed) */
static char *ve2_str_of(const json_t *v) { return strdup(ve2_sval(v, "")); }
