/*
 * code_execution_lifecycle.c -- real C11 port of the static judgement half of
 * upstream Hermes tools/code_execution_lifecycle.py.
 *
 * PoP: w20_codeexecut_b10023_is_inert_python @ tools/code_execution_lifecycle.py:_is_inert_python
 * PoP: w20_codeexecut_b10023_rejection @ tools/code_execution_lifecycle.py:lifecycle_guard_rejection
 * PoP: w20_codeexecut_b10023_inert_import @ tools/code_execution_lifecycle.py:_INERT_IMPORTS
 * PoP: w20_codeexecut_b10023_safe_builtin @ tools/code_execution_lifecycle.py:_SAFE_BUILTINS
 * PoP: w20_codeexecut_b10023_sink_attr @ tools/code_execution_lifecycle.py:_SINK_ATTRS
 * PoP: w20_codeexecut_b10023_max_match_chars @ tools/code_execution_lifecycle.py:_MAX_MATCH_CHARS
 *
 * _is_inert_python walks a CPython AST; this port scans the same source with a
 * tokenizer that recognises exactly the straight-line grammar the module
 * accepts (_STRAIGHT_LINE_STMTS: assign / annotated assign with a value /
 * expression / import / from-import / pass / with), rejecting every construct
 * that belongs to _SCOPE_OR_FLOW_NODES, every name load not bound by an earlier
 * top-level statement or in _SAFE_BUILTINS, and every attribute named in
 * _SINK_ATTRS / _INTROSPECTION_ATTRS or under their prefixes.
 *
 * The rest of the module -- the cron.lifecycle_guard raw-text scan
 * (contains_gateway_lifecycle_command, contains_host_interpreter_kill,
 * first_gateway_lifecycle_match) -- is bound to another module's regex tables
 * and stays upstream; its two predicates are arguments here.
 */
#include "hermes_json.h"
#include "noc_w20_codeexecut_b10023.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CEV_MAX_TOK 512
#define CEV_MAX_BOUND 256

static char *cev_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *cev_dupn(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* --- the three tables ------------------------------------------------------ */

static const char *const CEV_INERT[] = {
    "base64", "collections", "csv", "dataclasses", "datetime", "decimal", "difflib", "enum",
    "fractions", "functools", "hashlib", "html", "itertools", "json", "math", "pathlib", "pprint",
    "random", "re", "statistics", "string", "textwrap", "time", "unicodedata", NULL
};

static const char *const CEV_SAFE[] = {
    "abs", "all", "any", "ascii", "bin", "bool", "bytearray", "bytes", "chr", "dict", "divmod",
    "enumerate", "filter", "float", "format", "frozenset", "hash", "hex", "int", "isinstance",
    "issubclass", "iter", "len", "list", "map", "max", "min", "next", "oct", "open", "ord", "pow",
    "print", "range", "repr", "reversed", "round", "set", "slice", "sorted", "str", "sum", "tuple",
    "zip", "None", "True", "False", "Exception", "ValueError", "KeyError", "IndexError",
    "TypeError", "OSError", "StopIteration", "RuntimeError", NULL
};

static const char *const CEV_SINK[] = {
    "os", "sys", "subprocess", "system", "popen", "spawn", "exec", "eval", "kill", "killpg",
    "run", "call", "check_call", "check_output", "Popen", "import_module", "builtins", NULL
};
static const char *const CEV_SINK_PREFIX[] = {
    "_", "exec", "spawn", "posix_spawn", "popen", "kill", "fork", "startfile",
    "gi_", "f_", "tb_", "cr_", "ag_", NULL
};
static const char *const CEV_INTRO_ATTR[] = {"get_type_hints", NULL};

/* Python keywords the tokenizer sees as bare identifiers; none of them is an
 * ast.Name, so they are never load-checked. */
static const char *const CEV_KEYWORDS[] = {
    "and", "or", "not", "in", "is", "if", "else", "for", "lambda", "await", "yield", "async",
    "def", "class", "with", "as", "from", "import", "pass", "del", "raise", "return", "global",
    "nonlocal", "assert", "while", "try", "except", "finally", "elif", "match", "case",
    NULL
};

static int cev_in_list(const char *s, const char *const *list) {
    int i;
    if (!s) return 0;
    for (i = 0; list[i]; i++) {
        if (!strcmp(s, list[i])) return 1;
    }
    return 0;
}

int w20_codeexecut_b10023_inert_import(const char *name) {
    return cev_in_list(name, CEV_INERT);
}

int w20_codeexecut_b10023_safe_builtin(const char *name) {
    return cev_in_list(name, CEV_SAFE);
}

int w20_codeexecut_b10023_max_match_chars(void) {
    return 120;
}

int w20_codeexecut_b10023_sink_attr(const char *name) {
    int i;
    if (!name) return 0;
    if (cev_in_list(name, CEV_SINK)) return 1;
    if (cev_in_list(name, CEV_INTRO_ATTR)) return 1;
    for (i = 0; CEV_SINK_PREFIX[i]; i++) {
        size_t n = strlen(CEV_SINK_PREFIX[i]);
        if (!strncmp(name, CEV_SINK_PREFIX[i], n)) return 1;
    }
    return 0;
}

/* --- tokens ---------------------------------------------------------------- */

enum { CEV_T_NAME = 1, CEV_T_NUM, CEV_T_STR, CEV_T_OP };

typedef struct {
    int kind;
    char *text;   /* name / number literal / string content / operator */
    int fstr;     /* string carried an f prefix (its {..} fields are code) */
} cev_tok;

static void cev_tok_free(cev_tok *t, int n) {
    int i;
    for (i = 0; i < n; i++) free(t[i].text);
}

static int cev_is_ident(int c) { return isalnum(c) || c == '_'; }

/* multi-char operators, longest first */
static const char *const CEV_OPS[] = {
    "**=", "//=", ">>=", "<<=", "...", "**", "//", "<<", ">>", "<=", ">=", "==", "!=", ":=",
    "->", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "@=", NULL
};

typedef struct {
    const char *s;
    size_t i;
} cev_cur;

/* Scan one string literal at c->i (which points at the prefix letters or the
 * quote). Advances on success. */
static int cev_scan_string(cev_cur *c, cev_tok *t) {
    char quote;
    int triple = 0, has_f = 0;
    size_t start, end;
    const char *s = c->s;
    size_t i = c->i;
    while (s[i] && strchr("rbfuRBFU", s[i])) {
        if (s[i] == 'f' || s[i] == 'F') has_f = 1;
        i++;
    }
    if (s[i] != '\'' && s[i] != '"') return -1;
    quote = s[i++];
    if (s[i] == quote && s[i + 1] == quote) { triple = 1; i += 2; }
    start = i;
    for (;;) {
        if (!s[i]) return -1;                       /* unterminated literal */
        if (s[i] == '\\') { i += 2; continue; }
        if (triple) {
            if (s[i] == quote && s[i + 1] == quote && s[i + 2] == quote) { end = i; i += 3; break; }
        } else {
            if (s[i] == quote) { end = i; i += 1; break; }
            if (s[i] == '\n') return -1;
        }
        i++;
    }
    t->kind = CEV_T_STR;
    t->fstr = has_f;
    t->text = cev_dupn(s + start, end - start);
    if (!t->text) return -1;
    c->i = i;
    return 0;
}

static int cev_tokenize(const char *s, cev_tok *out, int cap, int *n_out) {
    cev_cur c;
    int n = 0;
    c.s = s;
    c.i = 0;
    while (s[c.i]) {
        unsigned char ch = (unsigned char)s[c.i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\\') { c.i++; continue; }
        if (ch == '#') break;                       /* comment to end of statement */
        if (n >= cap - 1) return -1;
        memset(&out[n], 0, sizeof(out[n]));
        if (isalpha(ch) || ch == '_') {
            size_t st = c.i;
            size_t j = c.i;
            while (s[j] && cev_is_ident((unsigned char)s[j])) j++;
            if ((s[j] == '\'' || s[j] == '"') && j - st <= 2) {
                int k, only_prefix = 1;
                for (k = (int)st; k < (int)j; k++) {
                    if (!strchr("rbfuRBFU", s[k])) { only_prefix = 0; break; }
                }
                if (only_prefix) {                      /* r"..." / rb'...' / f"..." */
                    c.i = st;
                    if (cev_scan_string(&c, &out[n]) != 0) return -1;
                    n++;
                    continue;
                }
            }
            out[n].kind = CEV_T_NAME;
            out[n].text = cev_dupn(s + st, j - st);
            if (!out[n].text) return -1;
            n++;
            c.i = j;
            continue;
        }
        if (isdigit(ch) || (ch == '.' && isdigit((unsigned char)s[c.i + 1]))) {
            size_t st = c.i;
            size_t j = c.i;
            while (s[j] && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '_')) j++;
            out[n].kind = CEV_T_NUM;
            out[n].text = cev_dupn(s + st, j - st);
            if (!out[n].text) return -1;
            n++;
            c.i = j;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            if (cev_scan_string(&c, &out[n]) != 0) return -1;
            n++;
            continue;
        }
        {
            int k, matched = 0;
            for (k = 0; CEV_OPS[k]; k++) {
                size_t len = strlen(CEV_OPS[k]);
                if (!strncmp(s + c.i, CEV_OPS[k], len)) {
                    out[n].kind = CEV_T_OP;
                    out[n].text = cev_dup(CEV_OPS[k]);
                    if (!out[n].text) return -1;
                    n++;
                    c.i += len;
                    matched = 1;
                    break;
                }
            }
            if (matched) continue;
            out[n].kind = CEV_T_OP;
            out[n].text = cev_dupn(s + c.i, 1);
            if (!out[n].text) return -1;
            n++;
            c.i++;
        }
    }
    *n_out = n;
    return 0;
}

/* --- the bound-name set ---------------------------------------------------- */

typedef struct {
    char *names[CEV_MAX_BOUND];
    int n;
} cev_bind;

static int cev_bind_has(const cev_bind *b, const char *name) {
    int i;
    for (i = 0; i < b->n; i++) {
        if (!strcmp(b->names[i], name)) return 1;
    }
    return 0;
}

static void cev_bind_add(cev_bind *b, const char *name) {
    int i;
    if (!name || !name[0] || b->n >= CEV_MAX_BOUND) return;
    for (i = 0; i < b->n; i++) {
        if (!strcmp(b->names[i], name)) return;
    }
    b->names[b->n] = cev_dup(name);
    if (b->names[b->n]) b->n++;
}

static void cev_bind_clear(cev_bind *b) {
    int i;
    for (i = 0; i < b->n; i++) free(b->names[i]);
    b->n = 0;
}

static void cev_bind_merge(cev_bind *dst, const cev_bind *src) {
    int i;
    for (i = 0; i < src->n; i++) cev_bind_add(dst, src->names[i]);
}

/* --- expression scan ------------------------------------------------------- */

static int cev_scan_fstring(const char *content, const cev_bind *b);
static int cev_check_tokens(cev_tok *t, int n, const cev_bind *b);

/* 0 = clean, 1 = reaches a process. */
static int cev_check_tokens(cev_tok *t, int n, const cev_bind *b) {
    int i, depth = 0;
    for (i = 0; i < n; i++) {
        if (t[i].kind == CEV_T_OP) {
            if (!strcmp(t[i].text, "(") || !strcmp(t[i].text, "[") || !strcmp(t[i].text, "{")) depth++;
            else if (!strcmp(t[i].text, ")") || !strcmp(t[i].text, "]") || !strcmp(t[i].text, "}")) depth--;
            continue;
        }
        if (t[i].kind == CEV_T_STR) {
            if (t[i].fstr && cev_scan_fstring(t[i].text, b)) return 1;
            continue;
        }
        if (t[i].kind != CEV_T_NAME) continue;
        if (i > 0 && t[i - 1].kind == CEV_T_OP && !strcmp(t[i - 1].text, ".")) {
            if (w20_codeexecut_b10023_sink_attr(t[i].text)) return 1;   /* attribute */
            continue;
        }
        if (cev_in_list(t[i].text, CEV_KEYWORDS)) continue;
        /* keyword argument: `f(name=...)` is not an ast.Name */
        if (i + 1 < n && t[i + 1].kind == CEV_T_OP && !strcmp(t[i + 1].text, "=") && depth > 0 &&
            i > 0 && t[i - 1].kind == CEV_T_OP &&
            (!strcmp(t[i - 1].text, "(") || !strcmp(t[i - 1].text, ","))) {
            continue;
        }
        if (w20_codeexecut_b10023_safe_builtin(t[i].text)) continue;
        if (cev_bind_has(b, t[i].text)) continue;
        return 1;
    }
    return 0;
}

/* {expr} fields of an f-string are parsed as expressions by CPython. */
static int cev_scan_fstring(const char *content, const cev_bind *b) {
    size_t i = 0;
    while (content[i]) {
        if (content[i] != '{') { i++; continue; }
        if (content[i + 1] == '{') { i += 2; continue; }
        {
            size_t st = ++i;
            int depth = 1;
            cev_tok toks[CEV_MAX_TOK];
            int nt = 0;
            while (content[i] && depth > 0) {
                if (content[i] == '{') depth++;
                else if (content[i] == '}') depth--;
                if (depth == 0) break;
                i++;
            }
            if (depth != 0) return 1;               /* unterminated field */
            {
                char *expr = cev_dupn(content + st, i - st);
                int bad;
                if (!expr) return 1;
                if (cev_tokenize(expr, toks, CEV_MAX_TOK, &nt) != 0) { free(expr); return 1; }
                bad = cev_check_tokens(toks, nt, b);
                cev_tok_free(toks, nt);
                free(expr);
                if (bad) return 1;
            }
            i++;
        }
    }
    return 0;
}

/* --- statements ------------------------------------------------------------ */

static int cev_match_close(cev_tok *t, int n, int open) {
    int depth = 0, k;
    const char *closer;
    if (t[open].kind != CEV_T_OP) return -1;
    closer = !strcmp(t[open].text, "(") ? ")" : !strcmp(t[open].text, "[") ? "]" : "}";
    for (k = open; k < n; k++) {
        if (t[k].kind != CEV_T_OP) continue;
        if (!strcmp(t[k].text, t[open].text)) depth++;
        else if (!strcmp(t[k].text, closer)) {
            depth--;
            if (depth == 0) return k;
        }
    }
    return -1;
}

/* Target list: binds names, checks Load bases and attributes. */
static int cev_scan_target(cev_tok *t, int n, const cev_bind *b, cev_bind *out) {
    int i = 0;
    while (i < n) {
        if (t[i].kind == CEV_T_OP && (!strcmp(t[i].text, "(") || !strcmp(t[i].text, "["))) {
            int close = cev_match_close(t, n, i);
            if (close < 0) return 1;
            if (cev_scan_target(t + i + 1, close - i - 1, b, out)) return 1;
            i = close + 1;
            continue;
        }
        if (t[i].kind == CEV_T_OP &&
            (!strcmp(t[i].text, "*") || !strcmp(t[i].text, ","))) { i++; continue; }
        if (t[i].kind != CEV_T_NAME) return 1;      /* not a legal target */
        if (cev_in_list(t[i].text, CEV_KEYWORDS)) return 1;
        {
            int j = i + 1;
            int is_ref = 0;
            while (j < n && t[j].kind == CEV_T_OP &&
                   (!strcmp(t[j].text, ".") || !strcmp(t[j].text, "[") || !strcmp(t[j].text, "("))) {
                is_ref = 1;
                if (!strcmp(t[j].text, ".")) {
                    if (j + 1 >= n || t[j + 1].kind != CEV_T_NAME) return 1;
                    if (w20_codeexecut_b10023_sink_attr(t[j + 1].text)) return 1;
                    cev_bind_add(out, t[i].text);   /* ast.walk(target) quirk */
                    j += 2;
                    continue;
                }
                {
                    int close = cev_match_close(t, n, j);
                    if (close < 0) return 1;
                    if (cev_check_tokens(t + j + 1, close - j - 1, b)) return 1;
                    j = close + 1;
                }
            }
            if (is_ref) {
                if (!w20_codeexecut_b10023_safe_builtin(t[i].text) && !cev_bind_has(b, t[i].text))
                    return 1;
            } else {
                cev_bind_add(out, t[i].text);
            }
            i = j;
        }
    }
    return 0;
}

static int cev_is_augassign(const char *op) {
    static const char *const AUG[] = {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "@=",
                                      "**=", "//=", ">>=", "<<=", NULL};
    return cev_in_list(op, AUG);
}

/* 0 = clean, 1 = may reach a process, 2 = a `with` header (caller handles). */
static int cev_scan_simple(cev_tok *t, int n, const cev_bind *b, cev_bind *bindings) {
    static const char *const COMPOUND[] = {
        "def", "class", "if", "elif", "else", "for", "while", "try", "except", "finally",
        "async", "match", "case", NULL
    };
    int i, depth, eq = -1;
    if (n <= 0) return 0;
    /* a decorated or compound statement is not straight-line */
    if (t[0].kind == CEV_T_OP) return 1;
    if (t[0].kind == CEV_T_NAME && cev_in_list(t[0].text, COMPOUND)) return 1;

    for (i = 0; i < n; i++) {
        if (t[i].kind == CEV_T_OP && (!strcmp(t[i].text, ":=") || !strcmp(t[i].text, "->")))
            return 1;                                /* NamedExpr / lambda arrow */
        if (t[i].kind != CEV_T_NAME) continue;
        if (!strcmp(t[i].text, "lambda") || !strcmp(t[i].text, "yield") ||
            !strcmp(t[i].text, "await") || !strcmp(t[i].text, "async") ||
            !strcmp(t[i].text, "def") || !strcmp(t[i].text, "class") ||
            !strcmp(t[i].text, "global") || !strcmp(t[i].text, "nonlocal") ||
            !strcmp(t[i].text, "del") || !strcmp(t[i].text, "raise") ||
            !strcmp(t[i].text, "assert") || !strcmp(t[i].text, "return") ||
            !strcmp(t[i].text, "break") || !strcmp(t[i].text, "continue"))
            return 1;
    }
    for (i = 0, depth = 0; i < n; i++) {
        if (t[i].kind == CEV_T_OP) {
            if (!strcmp(t[i].text, "(") || !strcmp(t[i].text, "[") || !strcmp(t[i].text, "{")) depth++;
            else if (!strcmp(t[i].text, ")") || !strcmp(t[i].text, "]") || !strcmp(t[i].text, "}")) depth--;
            continue;
        }
        if (t[i].kind == CEV_T_NAME && depth > 0 && !strcmp(t[i].text, "for")) return 1;  /* comprehension */
    }

    if (t[0].kind == CEV_T_NAME) {
        const char *w = t[0].text;
        if (!strcmp(w, "import") || !strcmp(w, "from")) {
            int from = !strcmp(w, "from");
            int k = 1;
            if (from) {
                if (k < n && t[k].kind == CEV_T_OP && !strcmp(t[k].text, ".")) return 1; /* level>0 */
                if (k >= n || t[k].kind != CEV_T_NAME) return 1;
                if (!w20_codeexecut_b10023_inert_import(t[k].text)) return 1;
                while (k < n && !(t[k].kind == CEV_T_NAME && !strcmp(t[k].text, "import"))) k++;
                if (k >= n) return 1;
                k++;
            } else {
                if (k >= n || t[k].kind != CEV_T_NAME) return 1;
                if (!w20_codeexecut_b10023_inert_import(t[k].text)) return 1;
                cev_bind_add(bindings, t[k].text);
                k++;
            }
            while (k < n) {
                if (t[k].kind == CEV_T_OP) {
                    if (!strcmp(t[k].text, ",") || !strcmp(t[k].text, "*") ||
                        !strcmp(t[k].text, ".") || !strcmp(t[k].text, "(") ||
                        !strcmp(t[k].text, ")")) { k++; continue; }
                    return 1;
                }
                if (t[k].kind != CEV_T_NAME) return 1;
                if (!from) {
                    char *top = cev_dup(t[k].text);
                    const char *asname = NULL;
                    if (!top) return 1;
                    k++;
                    while (k + 1 < n && t[k].kind == CEV_T_OP && !strcmp(t[k].text, ".") &&
                           t[k + 1].kind == CEV_T_NAME) k += 2;
                    if (k + 1 < n && t[k].kind == CEV_T_NAME && !strcmp(t[k].text, "as") &&
                        t[k + 1].kind == CEV_T_NAME) {
                        asname = t[k + 1].text;
                        k += 2;
                    }
                    if (!w20_codeexecut_b10023_inert_import(top)) { free(top); return 1; }
                    cev_bind_add(bindings, asname ? asname : top);
                    free(top);
                    continue;
                }
                {
                    const char *name = t[k].text;
                    const char *asname = NULL;
                    k++;
                    if (k + 1 < n && t[k].kind == CEV_T_NAME && !strcmp(t[k].text, "as") &&
                        t[k + 1].kind == CEV_T_NAME) {
                        asname = t[k + 1].text;
                        k += 2;
                    }
                    cev_bind_add(bindings, asname ? asname : name);
                }
            }
            return 0;
        }
        if (!strcmp(w, "pass") && n == 1) return 0;
        if (!strcmp(w, "with")) return 2;
    }

    depth = 0;
    for (i = 0; i < n; i++) {
        if (t[i].kind != CEV_T_OP) continue;
        if (!strcmp(t[i].text, "(") || !strcmp(t[i].text, "[") || !strcmp(t[i].text, "{")) { depth++; continue; }
        if (!strcmp(t[i].text, ")") || !strcmp(t[i].text, "]") || !strcmp(t[i].text, "}")) { depth--; continue; }
        if (depth != 0) continue;
        if (cev_is_augassign(t[i].text)) return 1;   /* AugAssign: not straight-line */
        if (!strcmp(t[i].text, "=") && eq < 0) eq = i;
    }
    if (eq == 0) return 1;
    if (eq > 0) {
        if (t[0].kind == CEV_T_NAME && n > 1 && t[1].kind == CEV_T_OP && !strcmp(t[1].text, ":")) {
            /* AnnAssign: NAME ':' annotation '=' value */
            if (eq > 2 && cev_check_tokens(t + 2, eq - 2, b)) return 1;
            if (cev_scan_target(t, 1, b, bindings)) return 1;
        } else if (cev_scan_target(t, eq, b, bindings)) {
            return 1;
        }
        if (cev_check_tokens(t + eq + 1, n - eq - 1, b)) return 1;
        return 0;
    }
    if (t[0].kind == CEV_T_NAME && n > 2 && t[1].kind == CEV_T_OP && !strcmp(t[1].text, ":")) {
        /* `x: int` -- AnnAssign with no value, binds nothing */
        if (cev_check_tokens(t + 2, n - 2, b)) return 1;
        return 0;
    }
    return cev_check_tokens(t, n, b) ? 1 : 0;
}

/* --- logical-line assembly ------------------------------------------------- */

typedef struct {
    int indent;
    char *text;
} cev_stmt;

typedef struct {
    cev_stmt *items;
    int n;
    int cap;
} cev_stmts;

static int cev_push_stmt(cev_stmts *v, int indent, char *text) {
    if (v->n >= v->cap) {
        int cap = v->cap ? v->cap * 2 : 16;
        cev_stmt *p = (cev_stmt *)realloc(v->items, (size_t)cap * sizeof(*p));
        if (!p) return -1;
        v->items = p;
        v->cap = cap;
    }
    v->items[v->n].indent = indent;
    v->items[v->n].text = text;
    v->n++;
    return 0;
}

static void cev_stmts_clear(cev_stmts *v) {
    int i;
    for (i = 0; i < v->n; i++) free(v->items[i].text);
    free(v->items);
    v->items = NULL;
    v->n = v->cap = 0;
}

/* Split source into logical statements, honouring strings, comments, brackets
 * and semicolons. -1 when the source cannot be a cell at all (unterminated
 * literal, unbalanced brackets). */
static int cev_split(const char *code, cev_stmts *out) {
    size_t i = 0, len = strlen(code);
    char *buf = (char *)malloc(len + 4);
    size_t bi = 0;
    int depth = 0, indent = 0, at_line_start = 1, stmt_indent = 0;
    char quote = 0;
    int triple = 0;
    if (!buf) return -1;

    while (i <= len) {
        char ch = code[i];
        if (at_line_start && ch != '\n' && ch != '\0') {
            size_t j = i;
            int col = 0;
            while (code[j] == ' ' || code[j] == '\t') {
                col += code[j] == '\t' ? 8 - (col % 8) : 1;
                j++;
            }
            indent = col;
            at_line_start = 0;
            if (code[j] == '#' || code[j] == '\n' || code[j] == '\0') {
                while (code[j] && code[j] != '\n') j++;
                i = j;
                if (code[i] == '\n') i++;
                at_line_start = 1;
                continue;
            }
            i = j;
            continue;
        }
        if (quote) {
            if (ch == '\0') { free(buf); return -1; }
            buf[bi++] = ch;
                if (ch == '\\') {
                if (!code[i + 1]) { free(buf); return -1; }
                buf[bi++] = code[i + 1];
                i += 2;
                continue;
            }
            if (triple) {
                if (ch == quote && code[i + 1] == quote && code[i + 2] == quote) {
                    buf[bi++] = quote;
                    buf[bi++] = quote;
                    i += 3;
                    quote = 0;
                    triple = 0;
                    continue;
                }
            } else if (ch == quote) {
                quote = 0;
            } else if (ch == '\n') {
                free(buf);
                return -1;
            }
            i++;
            continue;
        }
        if (ch == '\0' || (ch == '\n' && depth == 0)) {
            if (bi > 0) {
                char *text;
                size_t k;
                buf[bi] = '\0';
                text = cev_dup(buf);
                if (!text) { free(buf); return -1; }
                for (k = bi; k > 0; k--) {
                    if (!isspace((unsigned char)text[k - 1])) break;
                }
                text[k] = '\0';
                if (cev_push_stmt(out, stmt_indent, text) != 0) { free(text); free(buf); return -1; }
                bi = 0;
            }
            if (ch == '\0') break;
            i++;
            at_line_start = 1;
            continue;
        }
        if (ch == '#') {
            while (code[i] && code[i] != '\n') i++;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            char q = ch;
            int tr = (code[i + 1] == q && code[i + 2] == q);
            if (bi == 0) stmt_indent = indent;
            buf[bi++] = ch;
            i++;
            if (tr) {
                buf[bi++] = q;
                buf[bi++] = q;
                i += 2;
                triple = 1;
            }
            quote = q;
            continue;
        }
        if (bi == 0) stmt_indent = indent;
        if (ch == '(' || ch == '[' || ch == '{') depth++;
        else if (ch == ')' || ch == ']' || ch == '}') {
            depth--;
            if (depth < 0) { free(buf); return -1; }
        }
        if (ch == ';' && depth == 0) {
            char *text = cev_dup(buf);
            size_t k;
            if (!text) { free(buf); return -1; }
            for (k = bi; k > 0; k--) {
                if (!isspace((unsigned char)text[k - 1])) break;
            }
            text[k] = '\0';
            if (cev_push_stmt(out, stmt_indent, text) != 0) { free(text); free(buf); return -1; }
            bi = 0;
            i++;
            continue;
        }
        buf[bi++] = ch;
        i++;
    }
    free(buf);
    return depth == 0 ? 0 : -1;
}

/* --- the pass driver ------------------------------------------------------- */

int w20_codeexecut_b10023_is_inert_python(const char *code) {
    cev_stmts st;
    cev_bind root;
    cev_bind frame[8];
    int frame_indent[8];
    int nframes = 0;
    int i, ok = 1;

    if (!code) return 0;
    memset(&st, 0, sizeof(st));
    memset(&root, 0, sizeof(root));
    memset(frame, 0, sizeof(frame));
    if (cev_split(code, &st) != 0) {
        cev_stmts_clear(&st);
        return 0;
    }
    for (i = 0; i < st.n && ok; i++) {
        cev_tok toks[CEV_MAX_TOK];
        int nt = 0, rc;
        cev_bind local;
        cev_bind *cur;

        memset(&local, 0, sizeof(local));
        while (nframes > 0 && st.items[i].indent <= frame_indent[nframes - 1]) {
            cev_bind *closed = &frame[nframes - 1];
            nframes--;
            cev_bind_merge(nframes ? &frame[nframes - 1] : &root, closed);
            cev_bind_clear(closed);
        }
        cur = nframes ? &frame[nframes - 1] : &root;
        if (nframes == 0 && st.items[i].indent > 0) { ok = 0; break; }

        if (cev_tokenize(st.items[i].text, toks, CEV_MAX_TOK, &nt) != 0) {
            cev_tok_free(toks, nt);
            ok = 0;
            break;
        }
        rc = cev_scan_simple(toks, nt, cur, &local);
        if (rc == 2) {
            int k, colon = -1, depth = 0;
            for (k = 0; k < nt; k++) {
                if (toks[k].kind != CEV_T_OP) continue;
                if (!strcmp(toks[k].text, "(") || !strcmp(toks[k].text, "[") ||
                    !strcmp(toks[k].text, "{")) depth++;
                else if (!strcmp(toks[k].text, ")") || !strcmp(toks[k].text, "]") ||
                         !strcmp(toks[k].text, "}")) depth--;
                else if (depth == 0 && !strcmp(toks[k].text, ":")) colon = k;
            }
            if (colon <= 1 || nframes >= 8) {
                ok = 0;                              /* no body, or too deep */
            } else {
                /* `with item[, item]*:` -- every item is `expr [as name]` */
                cev_bind inner;
                int start = 1, k2, depth2 = 0;
                memset(&inner, 0, sizeof(inner));
                for (k2 = 1; k2 <= colon && ok; k2++) {
                    int boundary;
                    if (k2 < colon && toks[k2].kind == CEV_T_OP) {
                        if (!strcmp(toks[k2].text, "(") || !strcmp(toks[k2].text, "[") ||
                            !strcmp(toks[k2].text, "{")) depth2++;
                        else if (!strcmp(toks[k2].text, ")") || !strcmp(toks[k2].text, "]") ||
                                 !strcmp(toks[k2].text, "}")) depth2--;
                    }
                    boundary = (k2 == colon) ||
                               (depth2 == 0 && toks[k2].kind == CEV_T_OP &&
                                !strcmp(toks[k2].text, ","));
                    if (boundary) {
                        int split = -1, q;
                        for (q = start; q < k2; q++) {
                            if (toks[q].kind == CEV_T_NAME && !strcmp(toks[q].text, "as")) {
                                split = q;
                                break;
                            }
                        }
                        if (split >= 0) {
                            if (cev_check_tokens(toks + start, split - start, cur)) { ok = 0; break; }
                            for (q = split + 1; q < k2; q++) {
                                if (toks[q].kind == CEV_T_NAME) cev_bind_add(&inner, toks[q].text);
                            }
                        } else if (cev_check_tokens(toks + start, k2 - start, cur)) {
                            ok = 0;
                            break;
                        }
                        start = k2 + 1;
                    }
                }
                if (ok) {
                    cev_bind_merge(&inner, cur);
                    frame_indent[nframes] = st.items[i].indent;
                    frame[nframes] = inner;
                    nframes++;
                } else {
                    cev_bind_clear(&inner);
                }
            }
        } else if (rc != 0) {
            ok = 0;
        } else {
            cev_bind_merge(cur, &local);
        }
        cev_bind_clear(&local);
        cev_tok_free(toks, nt);
    }
    cev_bind_clear(&root);
    for (i = 0; i < nframes; i++) cev_bind_clear(&frame[i]);
    cev_stmts_clear(&st);
    return ok ? 1 : 0;
}

char *w20_codeexecut_b10023_rejection(const char *code, int contains_command,
                                      int contains_host_kill, const char *matched,
                                      const char *host_kill_text) {
    static const char *const HEAD =
        "Blocked: cannot restart or stop the gateway from inside the gateway process. The "
        "gateway would kill this script before it could complete (SIGTERM propagates to child "
        "processes).";
    static const char *const TAIL =
        " Run the lifecycle command from a shell outside the gateway. If the script only prints "
        "or writes that text as documentation, write it as straight-line top-level code (no def, "
        "lambda, class, loop, if, try or comprehension) that imports only pure stdlib modules "
        "(json, pathlib, re, textwrap, ...) and calls no process/shell/dynamic-code API (os, "
        "subprocess, exec, terminal, helpers from earlier cells); it is then treated as inert "
        "data.";
    size_t cap;
    char *out;

    if (!contains_command || w20_codeexecut_b10023_is_inert_python(code)) return NULL;
    if (contains_host_kill) {
        /* HOST_INTERPRETER_KILL_REJECTION is a constant owned by
         * cron.lifecycle_guard; the caller passes it through unchanged. */
        return cev_dup(host_kill_text ? host_kill_text : "");
    }
    cap = strlen(HEAD) + strlen(TAIL) + 64;
    if (matched && matched[0]) cap += strlen(matched);
    out = (char *)malloc(cap);
    if (!out) return NULL;
    if (matched && matched[0]) {
        size_t keep = strlen(matched);
        if (keep > (size_t)w20_codeexecut_b10023_max_match_chars())
            keep = (size_t)w20_codeexecut_b10023_max_match_chars();
        snprintf(out, cap, "%s Matched `%.*s` in the script.%s", HEAD, (int)keep, matched, TAIL);
    } else {
        snprintf(out, cap, "%s%s", HEAD, TAIL);
    }
    return out;
}
