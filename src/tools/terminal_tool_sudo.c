/*
 * terminal_tool_sudo.c - faithful C11 port of the quote-aware shell scanner and
 * sudo/background rewrites from tools/terminal_tool_sudo.py (gate w11_terminalto_3baf4c).
 *
 * PoP: w11_terminalto_3baf4c_looks_like_env_assignment @ tools/terminal_tool_sudo.py:_looks_like_env_assignment
 * PoP: w11_terminalto_3baf4c_read_shell_token @ tools/terminal_tool_sudo.py:_read_shell_token
 * PoP: w11_terminalto_3baf4c_scan_shell @ tools/terminal_tool_sudo.py:_scan_shell
 * PoP: w11_terminalto_3baf4c_rewrite_sudo @ tools/terminal_tool_sudo.py:_rewrite_real_sudo_invocations
 * PoP: w11_terminalto_3baf4c_count_sudo @ tools/terminal_tool_sudo.py:_count_real_sudo_invocations
 * PoP: w11_terminalto_3baf4c_rewrite_compound_background @ tools/terminal_tool_sudo.py:_rewrite_compound_background
 * PoP: w11_terminalto_3baf4c_sudo_wrong_password @ tools/terminal_tool_sudo.py:_sudo_wrong_password_failure
 *
 * TTY prompting, secret scopes and process execution are intentionally not ported.
 */
#include "noc_w11_terminalto_3baf4c.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char ASKPASS_PREFIX[] = "SUDO_ASKPASS=\"$__hermes_sudo_dir/a\" ";
static const char ASKPASS_SUFFIX[] = " \"${__hermes_sudo_o[@]}\"";

typedef struct { char *s; size_t n, cap; } SB;
static void sb_init(SB *b) { b->cap = 64; b->n = 0; b->s = (char *)malloc(b->cap); if (b->s) b->s[0] = 0; }
static void sb_reserve(SB *b, size_t extra) {
    if (!b->s || b->n + extra + 1 <= b->cap) return;
    size_t nc = b->cap * 2;
    while (nc < b->n + extra + 1) nc *= 2;
    char *q = (char *)realloc(b->s, nc);
    if (q) { b->s = q; b->cap = nc; }
}
static void sb_addn(SB *b, const char *p, size_t n) {
    if (!p || !n) return;
    sb_reserve(b, n);
    if (!b->s) return;
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = 0;
}
static void sb_add(SB *b, const char *p) { if (p) sb_addn(b, p, strlen(p)); }
static void sb_addc(SB *b, char c) { sb_addn(b, &c, 1); }
static char *sb_take(SB *b) { char *s = b->s ? b->s : (char *)malloc(1); if (s && !b->s) s[0] = 0; b->s = NULL; b->n = b->cap = 0; return s; }
static void sb_free(SB *b) { free(b->s); b->s = NULL; b->n = b->cap = 0; }

static char *substr(const char *s, size_t a, size_t b) {
    char *o = (char *)malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = 0; }
    return o;
}

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* ^[A-Za-z_][A-Za-z0-9_]*$ */
static int valid_env_name(const char *s, size_t n) {
    if (!n) return 0;
    unsigned char c = (unsigned char)s[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) return 0;
    for (size_t i = 1; i < n; i++) {
        c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

json_t *w11_terminalto_3baf4c_looks_like_env_assignment(const char *token) {
    if (!token) return json_bool(false);
    const char *eq = strchr(token, '=');
    if (!eq || eq == token) return json_bool(false);
    return json_bool(valid_env_name(token, (size_t)(eq - token)) ? true : false);
}

/* _SHELL_WORD_RE: quote runs, backslash escapes, or [^\s;|&()] chars. */
static size_t token_end(const char *s, size_t n, size_t i) {
    size_t p = i;
    while (p < n) {
        char c = s[p];
        if (c == '\'') {
            p++;
            while (p < n && s[p] != '\'') p++;
            if (p < n) p++;
        } else if (c == '"') {
            p++;
            while (p < n && s[p] != '"') {
                if (s[p] == '\\' && p + 1 < n) p += 2;
                else p++;
            }
            if (p < n && s[p] == '"') p++;
        } else if (c == '\\') {
            p += (p + 1 < n) ? 2 : 1;
        } else if (py_isspace((unsigned char)c) || strchr(";|&()", c)) {
            break;
        } else {
            p++;
        }
    }
    return p;
}

json_t *w11_terminalto_3baf4c_read_shell_token(const char *command, int start) {
    const char *s = command ? command : "";
    size_t n = strlen(s), i = (start < 0) ? 0 : (size_t)start;
    if (i > n) i = n;
    size_t e = token_end(s, n, i);
    json_t *arr = json_array();
    json_append(arr, json_string(substr(s, i, e)));
    json_append(arr, json_number((double)e));
    return arr;
}

/* ---------- scanner ---------- */
typedef struct { int at_start, parens, braces; } ScanState;

static int shell_next(const char *s, size_t n, size_t *i, int background, ScanState *st,
                      const char **kind, size_t *kstart, size_t *kend, int *was_start) {
    if (*i >= n) return 0;
    size_t pos = *i;
    char ch = s[pos];
    int grouped = st->parens || st->braces;
    int was = st->at_start;
    const char *k;
    size_t end;
    if (py_isspace((unsigned char)ch)) {
        k = grouped ? "skip" : "ws";
        end = pos + 1;
        if (ch == '\n') st->at_start = 1;
    } else if (ch == '#') {
        size_t p = pos;
        while (p < n && s[p] != '\n') p++;
        k = "comment"; end = p;
    } else if (background && ch == '\\' && pos + 1 < n) {
        k = "escape"; end = pos + 2;
    } else if (background && (ch == '\'' || ch == '"')) {
        k = "word"; end = token_end(s, n, pos);
    } else if (background && (ch == '(' || ch == ')' ||
               (ch == '{' && pos + 1 < n && py_isspace((unsigned char)s[pos + 1])) ||
               (ch == '}' && st->braces))) {
        if (ch == '(') st->parens++;
        else if (ch == ')') { if (st->parens > 0) st->parens--; }
        else if (ch == '{') st->braces++;
        else if (st->braces > 0) st->braces--;
        k = "op"; end = pos + 1;
    } else if (background && grouped) {
        k = "skip"; end = pos + 1;
    } else {
        int two = (pos + 1 < n) &&
            ((ch == '&' && s[pos + 1] == '&') || (ch == '|' && s[pos + 1] == '|') ||
             (background ? (ch == '&' && s[pos + 1] == '>') : (ch == ';' && s[pos + 1] == ';')));
        if (two) {
            k = "op"; end = pos + 2; st->at_start = 1;
        } else if (strchr(";|&()", ch)) {
            k = "op"; end = pos + 1; st->at_start = ch != ')' ? 1 : 0;
        } else {
            end = token_end(s, n, pos);
            char *tok = substr(s, pos, end);
            k = "word";
            st->at_start = (st->at_start && tok && strchr(tok, '=') && tok[0] != '=' &&
                            valid_env_name(tok, (size_t)(strchr(tok, '=') - tok))) ? 1 : 0;
            free(tok);
        }
    }
    *kind = k; *kstart = pos; *kend = end; *was_start = was; *i = end;
    return 1;
}

json_t *w11_terminalto_3baf4c_scan_shell(const char *command, int background) {
    const char *s = command ? command : "";
    size_t n = strlen(s), i = 0;
    ScanState st = {1, 0, 0};
    json_t *arr = json_array();
    const char *kind; size_t a, b; int was;
    while (shell_next(s, n, &i, background, &st, &kind, &a, &b, &was)) {
        json_t *ev = json_array();
        json_append(ev, json_string(kind));
        json_append(ev, json_number((double)a));
        json_append(ev, json_number((double)b));
        json_append(ev, json_bool(was ? true : false));
        json_append(arr, ev);
    }
    return arr;
}

/* ---------- shlex.split (single-token subset) ---------- */
/* Returns 1 for exactly one word, 0 for none/many, -1 for unclosed quote. */
static int shlex_one(const char *text, char **word) {
    size_t n = strlen(text), i = 0;
    while (i < n && py_isspace((unsigned char)text[i])) i++;
    if (i >= n) return 0;
    SB b; sb_init(&b);
    while (i < n && !py_isspace((unsigned char)text[i])) {
        char c = text[i];
        if (c == '\'' || c == '"') {
            char q = c; i++;
            while (i < n && text[i] != q) {
                if (q == '"' && text[i] == '\\' && i + 1 < n) { sb_addc(&b, text[i + 1]); i += 2; }
                else { sb_addc(&b, text[i]); i++; }
            }
            if (i >= n) { sb_free(&b); return -1; }
            i++;
        } else if (c == '\\' && i + 1 < n) {
            sb_addc(&b, text[i + 1]); i += 2;
        } else {
            sb_addc(&b, c); i++;
        }
    }
    while (i < n && py_isspace((unsigned char)text[i])) i++;
    if (i < n) { sb_free(&b); return 0; }
    *word = sb_take(&b);
    sb_free(&b);
    return 1;
}

static int has_special(const char *text) {
    return strpbrk(text, "$`*?[]~<>{}\"") != NULL;
}

static int in_strs(const char *w, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(w, set[i])) return 1;
    return 0;
}

static const char *const VALUE_OPTS[] = {"-u", "--unset", "-C", "--chdir", "-a", "--argv0"};
static const char *const FLAG_OPTS[] = {"-i", "--ignore-environment", "-0", "--null", "-v", "--debug"};

typedef struct { char **v; size_t n, cap; } Segs;
static void segs_add(Segs *g, const char *s, size_t n) {
    if (g->n == g->cap) {
        size_t nc = g->cap ? g->cap * 2 : 8;
        char **p = (char **)realloc(g->v, nc * sizeof(char *));
        if (!p) return;
        g->v = p; g->cap = nc;
    }
    if (g->n >= g->cap) return;
    char *d = (char *)malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = 0; }
    g->v[g->n++] = d;
}
static void segs_free(Segs *g) { for (size_t i = 0; i < g->n; i++) free(g->v[i]); free(g->v); }

/* The full rewrite is implemented here (split out to keep shell_next and the
   segment logic in one translation unit). */
static void rewrite_full(const char *command, int askpass, SB *out, int *count) {
    const char *s = command ? command : "";
    size_t n = strlen(s), i = 0;
    ScanState st = {1, 0, 0};
    Segs g = {0};
    int in_env = 0, env_operand = 0, env_options = 0;
    const char *kind; size_t a, b; int was;
    *count = 0;
    while (shell_next(s, n, &i, 0, &st, &kind, &a, &b, &was)) {
        char *text = substr(s, a, b);
        segs_add(&g, text ? text : "", b - a);
        free(text);
        size_t last = g.n - 1;
        if (!strcmp(kind, "op") || (!strcmp(kind, "ws") && b - a == 1 && s[a] == '\n')) {
            in_env = env_operand = env_options = 0;
        }
        if (strcmp(kind, "word") || !(was || in_env)) continue;
        const char *segtext = g.v[last];
        char *word = NULL;
        int cnt = shlex_one(segtext, &word);
        if (cnt < 0) { in_env = 0; continue; }
        if (cnt != 1 || has_special(segtext)) { in_env = 0; free(word); continue; }
        int skip = 0;
        if (in_env) {
            if (env_operand) { env_operand = 0; skip = 1; }
            else if (env_options && (!strcmp(word, "-") || !strcmp(word, "--"))) { env_options = 0; skip = 1; }
            else if (env_options && in_strs(word, VALUE_OPTS, sizeof(VALUE_OPTS) / sizeof(VALUE_OPTS[0]))) { env_operand = 1; skip = 1; }
            else {
                for (size_t k = 0; k < sizeof(FLAG_OPTS) / sizeof(FLAG_OPTS[0]); k++)
                    if (!strcmp(word, FLAG_OPTS[k])) skip = 1;
                for (size_t k = 0; k < sizeof(VALUE_OPTS) / sizeof(VALUE_OPTS[0]); k++)
                    if (VALUE_OPTS[k][0] == '-' && VALUE_OPTS[k][1] == '-' &&
                        strncmp(word, VALUE_OPTS[k], strlen(VALUE_OPTS[k])) == 0 &&
                        word[strlen(VALUE_OPTS[k])] == '=') skip = 1;
                static const char *const SHORT_OPTS[] = {"-u", "-C", "-a"};
                for (size_t k = 0; k < 3; k++) {
                    const char *opt = SHORT_OPTS[k];
                    if (strncmp(word, opt, strlen(opt)) == 0 && strlen(word) > 2) skip = 1;
                }
                if (!skip && env_options && word[0] == '-') { in_env = 0; skip = 1; }
                if (!skip && strchr(word, '=') && word[0] != '=' &&
                    valid_env_name(word, (size_t)(strchr(word, '=') - word))) { env_options = 0; skip = 1; }
            }
        } else if (strchr(segtext, '=') && segtext[0] != '=' &&
                   valid_env_name(segtext, (size_t)(strchr(segtext, '=') - segtext))) {
            skip = 1;
        }
        if (skip) { free(word); continue; }
        const char *slash = strrchr(word, '/');
        const char *exe = slash ? slash + 1 : word;
        in_env = !strcmp(exe, "env") ? 1 : 0;
        env_options = in_env;
        if (!strcmp(exe, "sudo")) {
            char *old = g.v[last];
            if (askpass) {
                size_t ol = strlen(old);
                char *p = (char *)malloc(sizeof(ASKPASS_PREFIX) + ol + sizeof(ASKPASS_SUFFIX));
                if (p) {
                    memcpy(p, ASKPASS_PREFIX, sizeof(ASKPASS_PREFIX) - 1);
                    memcpy(p + sizeof(ASKPASS_PREFIX) - 1, old, ol);
                    memcpy(p + sizeof(ASKPASS_PREFIX) - 1 + ol, ASKPASS_SUFFIX, sizeof(ASKPASS_SUFFIX));
                    free(old); g.v[last] = p;
                }
            } else {
                const char *add = " -S -p ''";
                size_t ol = strlen(old), al = strlen(add);
                char *p = (char *)realloc(old, ol + al + 1);
                if (p) { memcpy(p + ol, add, al + 1); g.v[last] = p; }
            }
            (*count)++;
        }
        free(word);
    }
    for (size_t k = 0; k < g.n; k++) sb_add(out, g.v[k]);
    segs_free(&g);
}

json_t *w11_terminalto_3baf4c_rewrite_sudo(const char *command, int askpass) {
    SB out; sb_init(&out);
    int count = 0;
    rewrite_full(command, askpass, &out, &count);
    char *s = sb_take(&out); sb_free(&out);
    json_t *arr = json_array();
    json_append(arr, json_string(s ? s : ""));
    json_append(arr, json_number((double)count));
    free(s);
    return arr;
}

json_t *w11_terminalto_3baf4c_count_sudo(const char *command) {
    SB out; sb_init(&out);
    int count = 0;
    rewrite_full(command, 0, &out, &count);
    sb_free(&out);
    return json_number((double)count);
}

/* ---------- compound background rewrite ---------- */
static char *concat_many(const char **parts, size_t n) {
    size_t total = 1;
    for (size_t i = 0; i < n; i++) total += strlen(parts[i]);
    char *o = (char *)malloc(total);
    if (!o) return NULL;
    o[0] = 0;
    for (size_t i = 0; i < n; i++) strcat(o, parts[i]);
    return o;
}

json_t *w11_terminalto_3baf4c_rewrite_compound_background(const char *command) {
    const char *s = command ? command : "";
    size_t n = strlen(s), i = 0;
    ScanState st = {1, 0, 0};
    size_t *chains = (size_t *)malloc((n + 1) * sizeof(size_t));
    size_t *amps = (size_t *)malloc((n + 1) * sizeof(size_t));
    size_t rw = 0;
    size_t chain_end = (size_t)-1;
    const char *kind; size_t a, b; int was;
    (void)was;
    while (shell_next(s, n, &i, 1, &st, &kind, &a, &b, &was)) {
        char *text = substr(s, a, b);
        if (!strcmp(kind, "op") && (!strcmp(text, "&&") || !strcmp(text, "||"))) chain_end = b;
        else if ((!strcmp(kind, "ws") && text[0] == '\n') ||
                 (!strcmp(kind, "op") && (!strcmp(text, ";") || !strcmp(text, "|") || !strcmp(text, "}")))) chain_end = (size_t)-1;
        else if (!strcmp(kind, "op") && !strcmp(text, "&")) {
            size_t j = a;
            while (j > 0 && py_isspace((unsigned char)s[j - 1])) j--;
            if (!(j > 0 && (s[j - 1] == '<' || s[j - 1] == '>'))) {
                if (chain_end != (size_t)-1) { chains[rw] = chain_end; amps[rw] = a; rw++; }
                chain_end = (size_t)-1;
            }
        }
        free(text);
    }
    char *result = strdup(s);
    for (size_t k = rw; k > 0; k--) {
        size_t ce = chains[k - 1], ap = amps[k - 1];
        size_t rlen = strlen(result);
        size_t ip = ce;
        while (ip < ap && ip < rlen && py_isspace((unsigned char)result[ip])) ip++;
        if (ip > ap) ip = ap;
        const char *suffix_src = ap + 1 <= rlen ? result + ap + 1 : result + rlen;
        const char *tail = suffix_src;
        while (*tail == ' ' || *tail == '\t') tail++;
        int needs = *tail && (!strchr(";\n&|)}", tail[0]) || !strncmp(tail, "&>", 2));
        const char *sep = needs ? " ;" : "";
        char *prefix = substr(result, 0, ip);
        char *middle = substr(result, ip, ap > rlen ? rlen : ap);
        char *suffix = strdup(suffix_src);
        const char *parts[6] = {prefix, "{ ", middle, "& }", sep, suffix};
        char *next = concat_many(parts, 6);
        free(prefix); free(middle); free(suffix); free(result);
        result = next ? next : strdup("");
    }
    free(chains); free(amps);
    json_t *r = json_string(result ? result : "");
    free(result);
    return r;
}

json_t *w11_terminalto_3baf4c_sudo_wrong_password(const char *output) {
    static const char *const markers[] = {
        "sudo: authentication failed", "sudo: incorrect password attempt",
        "sudo: maximum 3 incorrect authentication attempts", "sudo: 3 incorrect password attempts"};
    char *low = NULL;
    const char *text = output ? output : "";
    size_t n = strlen(text);
    low = (char *)malloc(n + 1);
    if (low) { for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)text[i]); low[n] = 0; }
    const char *hay = low ? low : text;
    int found = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (strstr(hay, markers[i])) { found = 1; break; }
    free(low);
    return json_bool(found ? true : false);
}
