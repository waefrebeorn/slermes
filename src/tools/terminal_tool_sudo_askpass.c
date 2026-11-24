/*
 * terminal_tool_sudo_askpass.c - C11 port of upstream
 * tools/terminal_tool_sudo_askpass.py (gate w23_terminalto_b54e19).
 *
 * PoP: w23_terminalto_b54e19_prologue @ tools/terminal_tool_sudo_askpass.py:build_prologue
 * PoP: w23_terminalto_b54e19_fill_timeout @ tools/terminal_tool_sudo_askpass.py:fill_timeout_token
 * PoP: w23_terminalto_b54e19_own_input @ tools/terminal_tool_sudo_askpass.py:_option_words_carry_own_input
 * PoP: w23_terminalto_b54e19_supported_once @ tools/terminal_tool_sudo_askpass.py:sudo_askpass_supported_once
 * PoP: w23_terminalto_b54e19_timeout_token @ tools/terminal_tool_sudo_askpass.py:SUDO_ASKPASS_TIMEOUT_TOKEN
 * PoP: w23_terminalto_b54e19_prefix @ tools/terminal_tool_sudo_askpass.py:SUDO_ASKPASS_PREFIX
 * PoP: w23_terminalto_b54e19_suffix @ tools/terminal_tool_sudo_askpass.py:SUDO_ASKPASS_SUFFIX
 * PoP: w23_terminalto_b54e19_capability_probe @ tools/terminal_tool_sudo_askpass.py:CAPABILITY_PROBE
 *
 * _PROLOGUE_TEMPLATE is carried byte-for-byte; the four placeholders are
 * substituted in the order Python substitutes them (checks, count, timeout,
 * background), each replacement over the whole string like str.replace.
 * Whether a backend may run CAPABILITY_PROBE, and running it, are the caller's
 * seams: this file owns the shell text decisions, not the process.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_terminalto_b54e19.h"

const char *w23_terminalto_b54e19_timeout_token(void) { return "__HERMES_SUDO_ASKPASS_T__"; }
const char *w23_terminalto_b54e19_prefix(void) { return "SUDO_ASKPASS=\"$__hermes_sudo_dir/a\" "; }
const char *w23_terminalto_b54e19_suffix(void) { return " \"${__hermes_sudo_o[@]}\""; }

const char *w23_terminalto_b54e19_capability_probe(void) {
    return "SUDO_ASKPASS=/bin/false sudo -A -V >/dev/null 2>&1 || exit 1\n"
           "for t in /bin/cat /bin/rm /bin/sleep; do [ -x \"$t\" ] || exit 1; done\n"
           "d=$(umask 077 && mktemp -d \"${TMPDIR:-/tmp}/hermes-sudo.XXXXXXXXXX\" 2>/dev/null) || exit 1\n"
           "case $d in *[\\\"\\\\\\$\\`]*) rm -rf -- \"$d\"; exit 1 ;; esac\n"
           "r=1\n"
           "if mkfifo \"$d/l\" 2>/dev/null && printf '#!/bin/sh\\nexec /bin/cat -- /dev/null\\n' > \"$d/a\""
           " && chmod 700 \"$d/a\"; then\n"
           "  ( exec 3<>\"$d/l\" ) 2>/dev/null && \"$d/a\" </dev/null >/dev/null 2>&1 && r=0\n"
           "fi\n"
           "rm -rf -- \"$d\"\n"
           "exit $r\n";
}

/* _PROLOGUE_HEAD: the first line of the template, the marker fill_timeout_token
 * looks for before it touches anything. */
#define SA_HEAD "__hermes_sudo_pw=; __hermes_sudo_o=(-S -p '')\n"

/* _PROLOGUE_TEMPLATE, one C string literal per source line. */
static const char *const SA_TEMPLATE =
    "__hermes_sudo_pw=; __hermes_sudo_o=(-S -p '')\n"
    "__hermes_sudo_dir=$(\\builtin umask 077 && \\command mktemp -d \"${TMPDIR:-/tmp}/hermes-sudo.XXXXXXXXXX\" 2>/dev/null) || __hermes_sudo_dir=\n"
    "case $__hermes_sudo_dir in *[\\\"\\\\\\$\\`]*) \\command rm -rf -- \"$__hermes_sudo_dir\" 2>/dev/null; __hermes_sudo_dir= ;; esac\n"
    "if \\builtin test -n \"$__hermes_sudo_dir\" &&\n"
    "@SUDO_CHECKS@  \\builtin test -x /bin/cat && \\builtin test -x /bin/rm && \\builtin test -x /bin/sleep &&\n"
    "  \\command mkfifo \"$__hermes_sudo_dir/p\" \"$__hermes_sudo_dir/l\" 2>/dev/null &&\n"
    "  \\builtin printf '#!/bin/sh\\nexit 0\\n' > \"$__hermes_sudo_dir/a\" 2>/dev/null &&\n"
    "  \\command chmod 700 \"$__hermes_sudo_dir/a\" 2>/dev/null &&\n"
    "  \"$__hermes_sudo_dir/a\" </dev/null >/dev/null 2>&1 &&\n"
    "  \\builtin printf '#!/bin/sh\\nd=\"%s\"\\nuntil (set -C; : > \"$d/k\") 2>/dev/null; do [ -d \"$d\" ] || exit 1; /bin/sleep 0.1 2>/dev/null || /bin/sleep 1; done\\n/bin/cat -- \"$d/p\" 2>/dev/null; r=$?\\n/bin/rm -f \"$d/k\"\\nexit $r\\n' \"$__hermes_sudo_dir\" >| \"$__hermes_sudo_dir/a\" 2>/dev/null && \\command chmod 700 \"$__hermes_sudo_dir/a\" 2>/dev/null &&\n"
    "  \\exec 19<>\"$__hermes_sudo_dir/l\"; then\n"
    "  IFS= \\builtin read -r __hermes_sudo_pw || \\builtin :\n"
    "  __hermes_sudo_i=1\n"
    "  while \\builtin test \"$__hermes_sudo_i\" -lt @COUNT@; do\n"
    "    IFS= \\builtin read -r __hermes_sudo_x || \\builtin break; __hermes_sudo_i=$((__hermes_sudo_i + 1))\n"
    "  done\n"
    "  \\builtin unset __hermes_sudo_x __hermes_sudo_i\n"
    "  __hermes_sudo_o=(-A)\n"
    "( ( \\exec 8<\"$__hermes_sudo_dir/l\" 19>&- </dev/null >/dev/null 2>&1\n"
    "    \\builtin trap '' HUP PIPE; \\builtin trap '\\builtin :' TERM\n"
    "    __hermes_sudo_n=@COUNT@\n"
    "    { __hermes_sudo_i=0\n"
    "  while \\builtin test \"$__hermes_sudo_i\" -lt \"$__hermes_sudo_n\"; do\n"
    "    __hermes_sudo_i=$((__hermes_sudo_i + 1))\n"
    "    { if \\builtin test \"$__hermes_sudo_i\" -lt \"$__hermes_sudo_n\"; then\n"
    "        ( \\command mkfifo \"$__hermes_sudo_dir/q\" ) && ( \\command mv -f \"$__hermes_sudo_dir/q\" \"$__hermes_sudo_dir/p\" )\n"
    "      else ( \\command rm -f \"$__hermes_sudo_dir/p\" ); fi >/dev/null 2>&1\n"
    "      \\builtin printf '%s\\n' \"$__hermes_sudo_pw\"; } > \"$__hermes_sudo_dir/p\" || \\builtin break\n"
    "  done; } 8<&- &\n"
    "    __hermes_sudo_w=$!\n"
    "    \\builtin unset __hermes_sudo_pw\n"
    "    __hermes_sudo_t=@TIMEOUT@; __hermes_sudo_g=0\n"
    "    while \\builtin :; do\n"
    "      SECONDS=0; \\builtin read -r -t 2 __hermes_sudo_x <&8; __hermes_sudo_r=$?\n"
    "      if \\builtin test \"$__hermes_sudo_r\" -le 128 && \\builtin test \"$SECONDS\" -lt 1; then \\builtin break; fi\n"
    "      if \\builtin kill -0 $$ 2>/dev/null; then \\builtin continue; fi\n"
    "      if \\builtin test @BG@ -eq 0; then \\builtin break; fi\n"
    "      __hermes_sudo_g=$((__hermes_sudo_g + 2))\n"
    "      if \\builtin test \"$__hermes_sudo_g\" -ge \"$__hermes_sudo_t\"; then \\builtin break; fi\n"
    "    done\n"
    "\\builtin kill \"$__hermes_sudo_w\" 2>/dev/null\n"
    "\\exec 7<>\"$__hermes_sudo_dir/p\" 2>/dev/null\n"
    "\\command rm -rf \"$__hermes_sudo_dir\"\n"
    "\\exec 7>&-\n"
    "  ) & )\n"
    "else\n"
    "  \\builtin test -z \"$__hermes_sudo_dir\" || \\command rm -rf -- \"$__hermes_sudo_dir\" 2>/dev/null\n"
    "  __hermes_sudo_dir=\n"
    "fi\n"
    "\\builtin unset __hermes_sudo_pw\n"
    ;

/* ---- small growable string ------------------------------------------------ */

typedef struct {
    char *s;
    size_t n;
    size_t cap;
} sa_buf;

static void sa_buf_init(sa_buf *b) { b->s = NULL; b->n = 0; b->cap = 0; }

static void sa_buf_free(sa_buf *b) {
    free(b->s);
    b->s = NULL;
    b->n = 0;
    b->cap = 0;
}

static int sa_reserve(sa_buf *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *grown;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 128;
    while (cap < need) {
        if (cap > (size_t)-1 / 2) return 0;
        cap *= 2;
    }
    grown = (char *)realloc(b->s, cap);
    if (!grown) return 0;
    b->s = grown;
    b->cap = cap;
    return 1;
}

static int sa_addn(sa_buf *b, const char *p, size_t n) {
    if (!sa_reserve(b, n)) return 0;
    if (n) memcpy(b->s + b->n, p, n);
    b->n += n;
    b->s[b->n] = '\0';
    return 1;
}

static int sa_add(sa_buf *b, const char *p) {
    return p ? sa_addn(b, p, strlen(p)) : 1;
}

static char *sa_take(sa_buf *b) {
    char *s = b->s;
    if (!s) {
        s = (char *)malloc(1);
        if (s) s[0] = '\0';
    }
    b->s = NULL;
    b->n = 0;
    b->cap = 0;
    return s;
}

static char *sa_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str.replace(needle, rep) over the whole string. */
static int sa_replace_all(sa_buf *out, const char *src, const char *needle, const char *rep) {
    size_t nl = strlen(needle);
    const char *p = src;
    if (!nl) return sa_add(out, src);
    for (;;) {
        const char *hit = strstr(p, needle);
        if (!hit) return sa_add(out, p);
        if (!sa_addn(out, p, (size_t)(hit - p))) return 0;
        if (!sa_add(out, rep)) return 0;
        p = hit + nl;
    }
}

/* Python truthiness of a JSON value; a missing node is falsy. */
static int sa_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    case JSON_ARRAY: return json_len(v) > 0;
    default: return 0;
    }
}

/* str() of a JSON scalar, the way "{word}".format(word=...) writes it. */
static char *sa_text(const json_t *v) {
    char buf[64];
    if (!v) return sa_dup("None");
    switch (v->type) {
    case JSON_NULL: return sa_dup("None");
    case JSON_BOOL: return sa_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return sa_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        long long n = (long long)v->num_val;
        if ((double)n == v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", n);
            return sa_dup(buf);
        }
        snprintf(buf, sizeof(buf), "%g", v->num_val);
        return sa_dup(buf);
    }
    default: return json_dumps(v, 0);
    }
}

/* ---- build_prologue ------------------------------------------------------ */

static int sa_check_one(sa_buf *out, const char *word) {
    return sa_add(out, "  ( SUDO_ASKPASS=/bin/false ") && sa_add(out, word) &&
           sa_add(out, " -A -V ) </dev/null >/dev/null 2>&1 &&\n");
}

/* One sudo and tool check per sudo command word; the default is ("sudo",). */
static int sa_checks(sa_buf *out, const json_t *words) {
    size_t i, n;
    if (!words || words->type != JSON_ARRAY) return sa_check_one(out, "sudo");
    n = json_len(words);
    for (i = 0; i < n; i++) {
        char *text = sa_text(json_get(words, i));
        int ok;
        if (!text) return 0;
        ok = sa_check_one(out, text);
        free(text);
        if (!ok) return 0;
    }
    return 1;
}

char *w23_terminalto_b54e19_prologue(long long count, const char *timeout_token,
                                     int background, const json_t *sudo_words) {
    sa_buf checks, a, b, c, d;
    char num[32];
    char *result = NULL;

    sa_buf_init(&checks);
    sa_buf_init(&a);
    sa_buf_init(&b);
    sa_buf_init(&c);
    sa_buf_init(&d);

    if (!sa_checks(&checks, sudo_words)) goto done;
    snprintf(num, sizeof(num), "%lld", count);
    if (!sa_replace_all(&a, SA_TEMPLATE, "@SUDO_CHECKS@", checks.s ? checks.s : "")) goto done;
    if (!sa_replace_all(&b, a.s, "@COUNT@", num)) goto done;
    if (!sa_replace_all(&c, b.s, "@TIMEOUT@", timeout_token ? timeout_token : "")) goto done;
    if (!sa_replace_all(&d, c.s, "@BG@", background ? "1" : "0")) goto done;
    result = sa_take(&d);
done:
    sa_buf_free(&checks);
    sa_buf_free(&a);
    sa_buf_free(&b);
    sa_buf_free(&c);
    sa_buf_free(&d);
    return result;
}

/* ---- fill_timeout_token -------------------------------------------------- */

static int sa_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* int(timeout): 1 on success, 0 where Python would raise (TypeError,
 * ValueError, OverflowError) and the caller falls back to 120 s. */
static int sa_int_arg(const json_t *v, long long *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:
        *out = v->bool_val ? 1 : 0;
        return 1;
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d != d) return 0;                        /* nan -> ValueError */
        if (d > 9.0e15 || d < -9.0e15) return 0;     /* beyond int64 -> OverflowError */
        *out = (long long)d;                         /* truncates toward zero */
        return 1;
    }
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long got;
        errno = 0;
        got = strtoll(s, &end, 10);
        if (end == s) return 0;          /* no digits -> ValueError */
        if (errno == ERANGE) return 0;   /* OverflowError */
        while (*end && sa_is_space(*end)) end++;
        if (*end) return 0;              /* trailing text -> ValueError */
        *out = got;
        return 1;
    }
    default:
        return 0;                        /* None/list/dict -> TypeError */
    }
}

char *w23_terminalto_b54e19_fill_timeout(const char *command, const json_t *timeout) {
    const char *token = w23_terminalto_b54e19_timeout_token();
    long long seconds;
    char tail[32];
    size_t head_len = strlen(SA_HEAD);
    const char *hit;
    sa_buf out;
    char *result;

    if (!command) return NULL;
    if (strncmp(command, SA_HEAD, head_len) != 0) return sa_dup(command);
    if (!sa_int_arg(timeout, &seconds)) seconds = W23_TERMINALTO_B54E19_FALLBACK_TIMEOUT_S;
    snprintf(tail, sizeof(tail), "%lld", seconds + W23_TERMINALTO_B54E19_SLACK_S);
    hit = strstr(command, token);
    if (!hit) return sa_dup(command);
    sa_buf_init(&out);
    if (!sa_addn(&out, command, (size_t)(hit - command))) {
        sa_buf_free(&out);
        return NULL;
    }
    if (!sa_add(&out, tail) || !sa_add(&out, hit + strlen(token))) {
        sa_buf_free(&out);
        return NULL;
    }
    result = sa_take(&out);
    sa_buf_free(&out);
    return result;
}

/* ---- _option_words_carry_own_input --------------------------------------- */

/* ^[0-9]*(?:[<>]|&>) */
static int sa_redirection(const char *raw) {
    size_t i = 0;
    while (raw[i] >= '0' && raw[i] <= '9') i++;
    if (raw[i] == '<' || raw[i] == '>') return 1;
    if (raw[i] == '&' && raw[i + 1] == '>') return 1;
    return 0;
}

/* ^[0-9]*(?:<<<|<<-?|<>|>>|>\||<|>)$ */
static int sa_bare_redirection(const char *raw) {
    static const char *const ops[] = {"<<<", "<<-", "<<", "<>", ">>", ">|", "<", ">"};
    const char *tail;
    size_t i = 0, k;
    while (raw[i] >= '0' && raw[i] <= '9') i++;
    tail = raw + i;
    for (k = 0; k < sizeof(ops) / sizeof(ops[0]); k++)
        if (strcmp(tail, ops[k]) == 0) return 1;
    return 0;
}

static int sa_value_short(char c) {
    return c && strchr("ugpCDrtUTRhc", c) != NULL;
}

static int sa_value_long(const char *word) {
    static const char *const names[] = {
        "--user", "--group", "--prompt", "--close-from", "--chdir", "--chroot",
        "--role", "--type", "--other-user", "--command-timeout", "--host",
        "--login-class"
    };
    size_t k;
    for (k = 0; k < sizeof(names) / sizeof(names[0]); k++)
        if (strcmp(word, names[k]) == 0) return 1;
    return 0;
}

/* shlex.split(word) in POSIX mode with comments off: *count words into *out,
 * or -1 where shlex raises ValueError.  Caller frees each word and the array. */
static int sa_shlex(const char *s, char ***out, size_t *count) {
    size_t len = strlen(s), i = 0, n = 0, cap = 4;
    char **words = (char **)malloc(cap * sizeof(char *));
    char *cur = (char *)malloc(len + 2);
    int bad = 0;
    if (!words || !cur) {
        free(words);
        free(cur);
        return -1;
    }
    for (;;) {
        size_t cn = 0;
        while (i < len && sa_is_space(s[i])) i++;
        if (i >= len) break;
        while (i < len && !sa_is_space(s[i])) {
            char c = s[i];
            if (c == '\\') {
                i++;
                if (i >= len) { bad = 1; break; }
                if (s[i] == '\n') { i++; continue; }
                cur[cn++] = s[i++];
            } else if (c == '\'') {
                i++;
                while (i < len && s[i] != '\'') cur[cn++] = s[i++];
                if (i >= len) { bad = 1; break; }
                i++;
            } else if (c == '"') {
                i++;
                while (i < len && s[i] != '"') {
                    if (s[i] == '\\' && i + 1 < len &&
                        (s[i + 1] == '"' || s[i + 1] == '\\' || s[i + 1] == '$' || s[i + 1] == '`')) {
                        cur[cn++] = s[i + 1];
                        i += 2;
                    } else if (s[i] == '\\' && i + 1 < len && s[i + 1] == '\n') {
                        i += 2;
                    } else {
                        cur[cn++] = s[i++];
                    }
                }
                if (i >= len) { bad = 1; break; }
                i++;
            } else {
                cur[cn++] = c;
                i++;
            }
        }
        if (bad) break;
        cur[cn] = '\0';
        if (n == cap) {
            char **grown = (char **)realloc(words, cap * 2 * sizeof(char *));
            if (!grown) { bad = 1; break; }
            words = grown;
            cap *= 2;
        }
        words[n] = sa_dup(cur);
        if (!words[n]) { bad = 1; break; }
        n++;
    }
    free(cur);
    if (bad) {
        for (i = 0; i < n; i++) free(words[i]);
        free(words);
        return -1;
    }
    *out = words;
    *count = n;
    return 0;
}

/* One raw word: 1 = an option word of sudo (keep scanning its options),
 * 0 = sudo's command word (the answer is False), -1 = an answer of True. */
static int sa_own_word_step(const char *raw, int *skip_value) {
    char **split = NULL;
    size_t wn = 0, k;
    int verdict = 1;

    if (*skip_value) {
        *skip_value = 0;
        return 1;
    }
    if (sa_redirection(raw)) {
        *skip_value = sa_bare_redirection(raw);
        return 1;
    }
    if (strchr(raw, '$') || strchr(raw, '`')) return -1; /* expansion: stdin form */
    if (sa_shlex(raw, &split, &wn) < 0) return -1;       /* ValueError: stdin form */
    if (wn != 1) {
        for (k = 0; k < wn; k++) free(split[k]);
        free(split);
        return -1;
    }
    {
        const char *word = split[0];
        if (strcmp(word, "--stdin") == 0 || strcmp(word, "--askpass") == 0) {
            verdict = -1;
        } else if (word[0] == '\0' || strcmp(word, "--") == 0 || word[0] != '-' || word[1] == '\0') {
            verdict = 0; /* sudo's command word starts here */
        } else if (word[1] == '-') {
            *skip_value = sa_value_long(word);
        } else {
            size_t j;
            for (j = 1; word[j]; j++) {
                char ch = word[j];
                if (ch == 'S' || ch == 'A') { verdict = -1; break; }
                if (sa_value_short(ch)) {
                    *skip_value = word[j + 1] == '\0';
                    break;
                }
            }
        }
    }
    for (k = 0; k < wn; k++) free(split[k]);
    free(split);
    return verdict;
}

json_t *w23_terminalto_b54e19_own_input(const json_t *words) {
    size_t i, n;
    int skip_value = 0;
    int carry = 0;
    if (!words || words->type != JSON_ARRAY) return json_bool(0);
    n = json_len(words);
    for (i = 0; i < n; i++) {
        char *raw = sa_text(json_get(words, i));
        int step;
        if (!raw) break;
        step = sa_own_word_step(raw, &skip_value);
        free(raw);
        if (step < 0) { carry = 1; break; }
        if (step == 0) break; /* return False at sudo's command word */
    }
    return json_bool(carry ? 1 : 0);
}

/* ---- sudo_askpass_supported_once ----------------------------------------- */

json_t *w23_terminalto_b54e19_supported_once(const json_t *world) {
    json_t *out = json_object();
    const json_t *w = (world && world->type == JSON_OBJECT) ? world : NULL;
    const json_t *cached = w ? json_obj_get(w, "cached") : NULL;
    int supported = 0;
    int probed = 0;
    int is_windows;

    if (cached && cached->type != JSON_NULL) {
        /* cached = getattr(env, "_sudo_askpass_ok", None): no probe, no rewrite */
        supported = sa_truthy(cached);
    } else if (sa_truthy(w ? json_obj_get(w, "runtime_checked") : NULL)) {
        supported = 1;
    } else if (sa_truthy(w ? json_obj_get(w, "probe_supported") : NULL)) {
        is_windows = 0;
        {
            const json_t *sysv = json_obj_get(w, "system");
            if (sysv && sysv->type == JSON_STRING && sysv->str_val &&
                strcmp(sysv->str_val, "Windows") == 0) is_windows = 1;
        }
        if (!(sa_truthy(json_obj_get(w, "is_local")) && is_windows)) {
            double rc = json_get_num(w, "run_rc", -1.0);
            probed = 1;
            supported = (!sa_truthy(json_obj_get(w, "run_raises")) && rc == 0.0) ? 1 : 0;
        }
    }

    json_set(out, "supported", json_bool(supported ? 1 : 0));
    json_set(out, "runs", json_number(probed ? 1.0 : 0.0));
    json_set(out, "timeout", probed ? json_number(json_get_num(w, "probe_timeout", 3.0)) : json_null());
    json_set(out, "cached_after", json_bool(supported ? 1 : 0));
    return out;
}
