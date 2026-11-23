/*
 * approval_floors.c — faithful C11 port of upstream Hermes tools/approval_floors.py.
 * PoP: w8_approvalflo_1e3a51_user_deny_block_result @ tools/approval_floors.py:_user_deny_block_result
 * PoP: w8_approvalflo_1e3a51_sudo_stdin_block_result @ tools/approval_floors.py:_sudo_stdin_block_result
 * PoP: w8_approvalflo_1e3a51_hardline_block_result @ tools/approval_floors.py:_hardline_block_result
 * PoP: w8_approvalflo_1e3a51_has_allowlist_shell_operator @ tools/approval_floors.py:_has_allowlist_shell_operator
 * PoP: w8_approvalflo_1e3a51_matching_allowlist_ex @ tools/approval_floors.py:_matching_permanent_allowlist_entry
 * PoP: w8_approvalflo_1e3a51_command_matches_allowlist_ex @ tools/approval_floors.py:_command_matches_permanent_allowlist
 * The ANSI-C expansion step is inlined (static copy of tools/approval_ansi_c.py port)
 * so this translation unit is self-contained for the allowlist operator scan.
 */
#include "hermes_json.h"
#include "noc_w8_approvalflo_1e3a51.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* growable byte buffer */
typedef struct { char *p; size_t n, cap; } fl_buf_t;
static void fl_bput(fl_buf_t *b, char c) {
    if (b->n + 2 > b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; b->p = realloc(b->p, nc); if (!b->p) return; b->cap = nc; }
    b->p[b->n++] = c; b->p[b->n] = 0;
}
static void fl_badd(fl_buf_t *b, const char *s, size_t len) {
    while (b->n + len + 1 > b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; if (nc < b->n + len + 1) nc = b->n + len + 1; b->p = realloc(b->p, nc); if (!b->p) return; b->cap = nc; }
    memcpy(b->p + b->n, s, len); b->n += len; b->p[b->n] = 0;
}
static void fl_bputs(fl_buf_t *b, const char *s) { fl_badd(b, s, strlen(s)); }
static void fl_bcodepoint(fl_buf_t *b, unsigned cp) {
    if (cp < 0x80) fl_bput(b, (char)cp);
    else if (cp < 0x800) { fl_bput(b, (char)(0xC0 | (cp >> 6))); fl_bput(b, (char)(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { fl_bput(b, (char)(0xE0 | (cp >> 12))); fl_bput(b, (char)(0x80 | ((cp >> 6) & 0x3F))); fl_bput(b, (char)(0x80 | (cp & 0x3F))); }
    else { fl_bput(b, (char)(0xF0 | (cp >> 18))); fl_bput(b, (char)(0x80 | ((cp >> 12) & 0x3F))); fl_bput(b, (char)(0x80 | ((cp >> 6) & 0x3F))); fl_bput(b, (char)(0x80 | (cp & 0x3F))); }
}
static int fl_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int fl_is_surrogate(unsigned c) { return c >= 0xD800 && c <= 0xDFFF; }

/* _ESCAPE_RE.sub(_decode_escape, body) then split at first NUL. */
static char *fl_decode_body(const char *body) {
    fl_buf_t lit = {0};
    size_t j = 0, n = strlen(body);
    while (j < n) {
        if (body[j] != '\\' || j + 1 >= n) { fl_bput(&lit, body[j++]); continue; }
        char c = body[j + 1];
        size_t consumed = 2;
        if (c == 'x' || c == 'u' || c == 'U') {
            size_t maxd = (c == 'x') ? 2 : (c == 'u') ? 4 : 8;
            size_t k = j + 2, cnt = 0; unsigned code = 0;
            while (cnt < maxd && k < n && fl_hexval(body[k]) >= 0) { code = code * 16 + (unsigned)fl_hexval(body[k]); k++; cnt++; }
            if (cnt) {
                if (code <= 0x10FFFF && !fl_is_surrogate(code)) fl_bcodepoint(&lit, code);
                else { fl_bput(&lit, '\\'); fl_bput(&lit, c); fl_badd(&lit, body + j + 2, cnt); }
                j = k; continue;
            }
            /* no digits: regex falls through to (.) with named = the letter */
        } else if (c >= '0' && c <= '7') {
            size_t k = j + 1, cnt = 0; unsigned code = 0;
            while (cnt < 3 && k < n && body[k] >= '0' && body[k] <= '7') { code = code * 8 + (unsigned)(body[k] - '0'); k++; cnt++; }
            fl_bcodepoint(&lit, code & 0xFF); j = k; continue;
        } else if (c == 'c' && j + 2 < n) {
            unsigned code = (unsigned char)body[j + 2];
            fl_bcodepoint(&lit, code & 0x1F); j += 3; continue;
        }
        /* named fallback */
        unsigned named = 0;
        switch (c) {
            case 'a': named = 0x07; break; case 'b': named = 0x08; break;
            case 'e': case 'E': named = 0x1B; break; case 'f': named = 0x0C; break;
            case 'n': named = 0x0A; break; case 'r': named = 0x0D; break;
            case 't': named = 0x09; break; case 'v': named = 0x0B; break;
            case '\\': named = 0x5C; break; case '\'': named = 0x27; break;
            case '"': named = 0x22; break; case '?': named = 0x3F; break;
            default: break;
        }
        if (named) fl_bcodepoint(&lit, named);
        else { fl_bput(&lit, '\\'); fl_bput(&lit, c); }
        j += 2;
    }
    /* bash ends the string at a decoded NUL */
    if (lit.p) {
        for (size_t i = 0; i < lit.n; i++) if (lit.p[i] == 0) { lit.n = i; lit.p[i] = 0; break; }
    }
    if (!lit.p) { lit.p = malloc(1); if (lit.p) lit.p[0] = 0; }
    return lit.p ? lit.p : strdup("");
}
static char *fl_single_quoted(const char *body) {
    char *lit = fl_decode_body(body);
    fl_buf_t b = {0};
    fl_bput(&b, '\'');
    for (const char *p = lit; *p; p++) {
        if (*p == '\'') fl_bputs(&b, "'\"'\"'");
        else fl_bput(&b, *p);
    }
    fl_bput(&b, '\'');
    free(lit);
    return b.p ? b.p : strdup("''");
}

/* Scan for the closing quote starting just after the opening quote. */
static long long fl_scan_span(const char *t, long long pos) {
    long long n = (long long)strlen(t);
    while (pos < n) {
        if (t[pos] == '\\') { if (pos + 1 < n) pos += 2; else pos++; }
        else if (t[pos] == '\'') return pos + 1;
        else pos++;
    }
    return -1;
}
long long fl_span_end(const char *text, long long start) { return fl_scan_span(text, start + 2); }

int fl_is_comment(const char *text, long long index) {
    long long n = (long long)strlen(text);
    if (index >= n || index < 0 || text[index] != '#') return 0;
    if (index == 0) return 1;
    char prev = text[index - 1];
    if (!(isspace((unsigned char)prev) || strchr(";&|()<>", prev))) return 0;
    long long j = index - 2;
    while (j >= 0 && text[j] == '\\') j--;
    return ((index - 2 - j) % 2) == 0;
}
long long fl_skip_cont(const char *text, long long i) {
    long long n = (long long)strlen(text);
    while (i + 1 < n && text[i] == '\\' && text[i + 1] == '\n') i += 2;
    return i;
}

typedef struct { char kind; long long depth; } fl_ctx_t;
char *fl_expand(const char *command) {
    if (!command) return strdup("");
    if (!strchr(command, '$')) return strdup(command);
    fl_buf_t out = {0};
    fl_ctx_t *st = malloc(64 * sizeof(*st));
    size_t sn = 1, scap = 64;
    st[0].kind = 'u'; st[0].depth = 0;
    long long i = 0, n = (long long)strlen(command);
    while (i < n) {
        char ch = command[i];
        fl_ctx_t *ctx = &st[sn - 1];
        if (i + 1 < n && ch == '\\' && command[i + 1] == '\n') { i += 2; continue; }
        if (ch == '\\') {
            fl_bput(&out, ch);
            if (i + 1 < n) fl_bput(&out, command[i + 1]);
            i += 2; continue;
        }
        long long after = (ch == '$') ? fl_skip_cont(command, i + 1) : i + 1;
        char following = (after < n) ? command[after] : '\0';
        if (ch == '$' && following == '$') { fl_bputs(&out, "$$"); i = after + 1; continue; }
        if (ctx->kind == '"') {
            if (ch == '"') { if (sn > 1) sn--; fl_bput(&out, ch); i++; continue; }
            if ((ch == '$' && following == '(') || ch == '`') {
                if (sn == scap) { scap *= 2; st = realloc(st, scap * sizeof(*st)); }
                st[sn].kind = (ch == '$') ? '(' : '`'; st[sn].depth = 0; sn++;
                fl_bputs(&out, (ch == '$') ? "$(" : "`");
                i = (ch == '$') ? after + 1 : i + 1; continue;
            }
            fl_bput(&out, ch); i++; continue;
        }
        if (ch == '$' && following == '\'') {
            long long end = fl_scan_span(command, after + 1);
            if (end > 0) {
                long long bs = after + 1, be = end - 1;
                size_t blen = (size_t)(be > bs ? be - bs : 0);
                char *body = malloc(blen + 1);
                if (body) { memcpy(body, command + bs, blen); body[blen] = 0; char *q = fl_single_quoted(body); fl_bputs(&out, q); free(q); free(body); }
                i = end; continue;
            }
        }
        if (ch == '$' && following == '"') { i = after; continue; }
        if (ch == '\'') {
            long long e = i + 1;
            while (e < n && command[e] != '\'') e++;
            long long end = (e < n) ? e + 1 : n;
            fl_badd(&out, command + i, (size_t)(end - i));
            i = end; continue;
        }
        if (ch == '#' && fl_is_comment(command, i)) {
            long long e = i;
            while (e < n && command[e] != '\n') e++;
            fl_badd(&out, command + i, (size_t)(e - i));
            i = e; continue;
        }
        if (ch == '"') { if (sn == scap) { scap *= 2; st = realloc(st, scap * sizeof(*st)); } st[sn].kind = '"'; st[sn].depth = 0; sn++; }
        else if (ctx->kind == '`' && ch == '`') { if (sn > 1) sn--; }
        else if (ctx->kind == '(' && ch == '(') { ctx->depth++; }
        else if (ctx->kind == '(' && ch == ')') { if (ctx->depth) ctx->depth--; else if (sn > 1) sn--; }
        fl_bput(&out, ch); i++;
    }
    free(st);
    if (!out.p) return strdup("");
    return out.p;
}
/* ---- approval_floors.py pre-gate floors ---- */
static const char *const FL_CTRL = "\n\r;&|<>`$()";

static char *fl_trim(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) n--;
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = 0; }
    return r ? r : strdup("");
}
static struct json_t *fl_msg_obj(const char *a, const char *b, const char *c, const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "approved", json_bool(false));
    if (a) json_set(o, a, json_bool(true));
    if (b) json_set(o, b, json_bool(true));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
struct json_t *w8_approvalflo_1e3a51_user_deny_block_result(const char *pattern) {
    char m[4096];
    snprintf(m, sizeof(m),
        "BLOCKED: this command matches the user-defined deny rule '%s' (approvals.deny in config.yaml). "
        "It cannot be executed via the agent \xE2\x80\x94 not even with --yolo, /yolo, or approvals.mode=off. "
        "Do NOT retry or rephrase this command; the user has explicitly forbidden it.",
        pattern ? pattern : "");
    return fl_msg_obj(NULL, "user_deny", NULL, m);
}
struct json_t *w8_approvalflo_1e3a51_sudo_stdin_block_result(const char *description) {
    char m[4096];
    snprintf(m, sizeof(m),
        "BLOCKED: %s. Do not pipe passwords to 'sudo -S' \xE2\x80\x94 this is a brute-force attack vector. "
        "Set SUDO_PASSWORD in your .env file if the agent needs passwordless sudo, or run the sudo command manually in your own terminal.",
        description ? description : "");
    return fl_msg_obj(NULL, NULL, NULL, m);
}
struct json_t *w8_approvalflo_1e3a51_hardline_block_result(const char *description, const char *command) {
    (void)command; /* recovery payload saving is deliberately out of scope for this pure port */
    char m[4096];
    snprintf(m, sizeof(m),
        "BLOCKED (hardline): %s. This command is on the unconditional blocklist and cannot be executed via the agent "
        "\xE2\x80\x94 not even with --yolo, /yolo, approvals.mode=off, or cron approve mode. If you genuinely need to "
        "run it, run it yourself in a terminal outside the agent.", description ? description : "");
    return fl_msg_obj("hardline", NULL, NULL, m);
}

/* fnmatch.fnmatchcase subset: * ? [seq] with ranges and !/^ negation */
static int fl_match_class(const char **pp, char c) {
    const char *p = *pp + 1;
    int neg = 0, hit = 0;
    if (*p == '!' || *p == '^') { neg = 1; p++; }
    while (*p && *p != ']') {
        if (p[1] == '-' && p[2] && p[2] != ']') { if (c >= p[0] && c <= p[2]) hit = 1; p += 3; }
        else { if (c == *p) hit = 1; p++; }
    }
    if (*p == ']') p++;
    *pp = p;
    return neg ? !hit : hit;
}
static int fl_glob(const char *p, const char *s) {
    const char *star = NULL, *ss = s;
    while (*s) {
        if (*p == '*') { star = p++; ss = s; continue; }
        if (*p == '?' || *p == *s) { p++; s++; continue; }
        if (*p == '[' && fl_match_class(&p, *s)) { s++; continue; }
        if (star) { p = star + 1; s = ++ss; continue; }
        return 0;
    }
    while (*p == '*') p++;
    return *p == 0;
}
static int fl_has_wild(const char *s) { return strpbrk(s, "*?[") != NULL; }

static int fl_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int fl_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }
static int fl_re_assign(const char *w) {
    if (!fl_ident_start(*w)) return 0;
    const char *p = w + 1;
    while (fl_ident_char(*p)) p++;
    return *p == '=';
}
static int fl_re_subscript_assign(const char *w) {
    if (!fl_ident_start(*w)) return 0;
    const char *p = w + 1;
    while (fl_ident_char(*p)) p++;
    if (*p != '[') return 0;
    const char *e = strchr(p, ']');
    if (!e) return 0;
    e++;
    if (*e == '+') e++;
    return *e == '=';
}
static int fl_has_reinterpreter(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        if (i != 0 && s[i-1] != ' ' && s[i-1] != '\t') continue;
        if (s[i] == '-' && i + 1 < n && s[i+1] == '-') {
            if (!strncmp(s + i, "--command", 9)) {
                char nx = s[i+9];
                if (nx == 0 || nx == ' ' || nx == '\t' || nx == '=') return 1;
            }
            if (!strncmp(s + i, "--eval", 6)) {
                char nx = s[i+6];
                if (nx == 0 || nx == ' ' || nx == '\t' || nx == '=') return 1;
            }
        } else if (s[i] == '-') {
            size_t j = i + 1;
            while (j < n && s[j] != '-' && s[j] != ' ' && s[j] != '\t' && s[j] != '\n' && s[j] != '\r' && s[j] != '=') j++;
            if (j > i + 1 && (s[j-1] == 'c' || s[j-1] == 'e') &&
                (j >= n || s[j] == ' ' || s[j] == '\t' || s[j] == '=')) return 1;
        }
    }
    return 0;
}
/* POSIX-ish shlex.split(..., posix=True) for the subscript-evaluator table. */
static char **fl_shlex(const char *s, int *cnt) {
    char **w = NULL; int n = 0, cap = 0;
    size_t i = 0, len = strlen(s);
    while (i < len) {
        while (i < len && isspace((unsigned char)s[i])) i++;
        if (i >= len) break;
        fl_buf_t cur = {0};
        char q = 0;
        while (i < len) {
            char ch = s[i];
            if (!q && isspace((unsigned char)ch)) break;
            if (!q && (ch == '\'' || ch == '"')) { q = ch; i++; continue; }
            if (q && ch == q) { q = 0; i++; continue; }
            if (ch == '\\' && i + 1 < len && q != '\'') { fl_bput(&cur, s[i+1]); i += 2; continue; }
            fl_bput(&cur, ch); i++;
        }
        if (n == cap) { cap = cap ? cap * 2 : 8; w = realloc(w, (size_t)(cap + 1) * sizeof(*w)); }
        w[n++] = cur.p ? cur.p : strdup("");
    }
    if (!w) w = malloc(sizeof(*w));
    w[n] = NULL;
    *cnt = n;
    return w;
}
static int fl_evaluates_quoted_subscript(const char *cmd) {
    /* bash joins line continuations before tokenizing */
    size_t n = strlen(cmd);
    char *clean = malloc(n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (cmd[i] == '\\' && i + 1 < n && cmd[i+1] == '\n') { i++; continue; }
        clean[k++] = cmd[i];
    }
    clean[k] = 0;
    int cnt = 0;
    char **words = fl_shlex(clean, &cnt);
    free(clean);
    int idx = 0;
    while (idx < cnt) {
        const char *w = words[idx];
        if (fl_re_subscript_assign(w)) { idx = -1; break; }
        if (!strcmp(w, "builtin") || !strcmp(w, "command") || !strcmp(w, "time") || !strcmp(w, "!")) {
            int opt_prefix = strcmp(w, "!") != 0;
            idx++;
            while (opt_prefix && idx < cnt && words[idx][0] == '-') idx++;
        } else if (fl_re_assign(w)) {
            idx++;
        } else break;
    }
    int res = 0;
    if (idx >= 0 && idx < cnt) {
        const char *first = words[idx];
        const char *required = NULL;
        int found = 1;
        if (!strcmp(first, "test") || !strcmp(first, "[") || !strcmp(first, "printf")) required = "-v";
        else if (!strcmp(first, "[[") || !strcmp(first, "unset") || !strcmp(first, "read") ||
                 !strcmp(first, "declare") || !strcmp(first, "typeset") || !strcmp(first, "local") ||
                 !strcmp(first, "let")) required = NULL;
        else found = 0;
        if (found) {
            if (required == NULL) res = 1;
            else {
                size_t rl = strlen(required);
                for (int j = idx + 1; j < cnt; j++) {
                    if (!strncmp(words[j], required, rl) || fl_glob(required, words[j])) { res = 1; break; }
                }
            }
        }
    }
    for (int i = 0; i < cnt; i++) free(words[i]);
    free(words);
    return res;
}
int w8_approvalflo_1e3a51_has_allowlist_shell_operator(const char *command) {
    char *expanded = fl_expand(command ? command : "");
    const char *s = expanded ? expanded : "";
    char quote = 0;
    int rein = 0, pe = 0;
    int testv = fl_evaluates_quoted_subscript(s);
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        char ch = s[i];
        if (ch == '\\' && quote != '\'') {
            char nxt = (i + 1 < n) ? s[i+1] : '\0';
            if (nxt && strchr(FL_CTRL, nxt)) rein = 1;
            i++;
            continue;
        }
        if (quote) {
            if (ch == quote) quote = 0;
            else if (quote == '"' && (ch == '`' || ch == '$')) { free(expanded); return 1; }
            else if (strchr(FL_CTRL, ch)) rein = 1;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
        } else if (ch == '$') {
            if (i + 1 < n && s[i+1] == '(') { free(expanded); return 1; }
            if (i + 1 < n && (s[i+1] == '{' || s[i+1] == '[')) pe = 1;
        } else if (ch == '{') {
            pe = 1;
        } else if (strchr(FL_CTRL, ch) && ch != '(' && ch != ')') {
            free(expanded); return 1;
        }
    }
    free(expanded);
    if (quote) return 1;
    if ((testv || pe) && rein) return 1;
    int answer = 0;
    if (rein) {
        char *again = fl_expand(command ? command : "");
        answer = fl_has_reinterpreter(again ? again : "");
        free(again);
    }
    return answer;
}

char *w8_approvalflo_1e3a51_matching_allowlist_ex(const char *command, const struct json_t *patterns) {
    char *cmd = fl_trim(command ? command : "");
    if (!*cmd || w8_approvalflo_1e3a51_has_allowlist_shell_operator(cmd)) { free(cmd); return NULL; }
    char *found = NULL;
    if (patterns && patterns->type == JSON_ARRAY) {
        size_t cnt = json_len(patterns);
        for (size_t i = 0; i < cnt && !found; i++) {
            const json_t *p = json_get(patterns, i);
            if (!p || p->type != JSON_STRING) continue;
            char *pp = fl_trim(p->str_val ? p->str_val : "");
            if (*pp && (!strcmp(cmd, pp) || (fl_has_wild(pp) && fl_glob(pp, cmd)))) found = strdup(pp);
            free(pp);
        }
    }
    free(cmd);
    return found;
}
int w8_approvalflo_1e3a51_command_matches_allowlist_ex(const char *command, const struct json_t *patterns) {
    char *p = w8_approvalflo_1e3a51_matching_allowlist_ex(command, patterns);
    if (p) { free(p); return 1; }
    return 0;
}