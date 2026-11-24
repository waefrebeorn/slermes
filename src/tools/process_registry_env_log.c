/*
 * process_registry_env_log.c -- real C11 port of the incremental sandbox log
 * poll of upstream Hermes tools/process_registry_env_log.py.
 *
 * PoP: w22_processreg_cd9f60_log_delta_command @ tools/process_registry_env_log.py:log_delta_command
 * PoP: w22_processreg_cd9f60_parse_log_delta_reply @ tools/process_registry_env_log.py:parse_log_delta_reply
 * PoP: w22_processreg_cd9f60_fallback_exit_code @ tools/process_registry_env_log.py:fallback_exit_code
 *
 * `log_delta_command` is reproduced byte for byte (the header line, the size
 * read before the tail, the offset reset on a shrunken file, the UTF-8
 * boundary walk-back, the final `echo` + spliced tail). The exec handle that
 * `fallback_exit_code` calls is the seam: its two `output` strings come in.
 */
#include "hermes_json.h"
#include "noc_w22_processreg_cd9f60.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- growable builder ---------- */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    int    oom;
} pd_buf;

static void pd_init(pd_buf *b) {
    b->cap = 1024;
    b->len = 0;
    b->oom = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) { b->oom = 1; b->cap = 0; return; }
    b->buf[0] = '\0';
}

static void pd_putn(pd_buf *b, const char *s, size_t n) {
    if (b->oom || !b->buf) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 1024;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) { b->oom = 1; return; }
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';               /* always NUL-terminated */
}

static void pd_puts(pd_buf *b, const char *s) {
    if (!s) s = "";
    pd_putn(b, s, strlen(s));
}

static void pd_putll(pd_buf *b, long long v) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    pd_puts(b, tmp);
}

char *w22_processreg_cd9f60_log_delta_command(const char *quoted_log_path, long long offset,
                                              const char *quoted_pid_path,
                                              const char *quoted_exit_path) {
    const char *log = quoted_log_path ? quoted_log_path : "None";
    const char *pid = quoted_pid_path ? quoted_pid_path : "None";
    const char *ex = quoted_exit_path ? quoted_exit_path : "None";
    pd_buf b;
    char *out;

    pd_init(&b);
    if (b.oom) return NULL;
    pd_puts(&b, "if kill -0 \"$(cat ");
    pd_puts(&b, pid);
    pd_puts(&b, " 2>/dev/null)\" 2>/dev/null; then T=run; else T=\"exit:$(cat ");
    pd_puts(&b, ex);
    pd_puts(&b, " 2>/dev/null | tr -dc '0-9')\"; fi; O=");
    pd_putll(&b, offset);
    pd_puts(&b, "; S=$({ wc -c < ");
    pd_puts(&b, log);
    pd_puts(&b, "; } 2>/dev/null | tr -dc '0-9'); S=${S:-0}; if [ \"$S\" -lt \"$O\" ]; "
                "then O=0; fi; case \"$T\" in exit:*) ;; *) N=0; P=$S; while [ \"$P\" "
                "-gt \"$O\" ] && [ \"$N\" -lt 3 ]; do B=$(tail -c +$P ");
    pd_puts(&b, log);
    pd_puts(&b, " 2>/dev/null | head -c 1 | od -An -to1 | tr -dc '0-9'); case \"$B\" in "
                "2[0-7][0-7]) P=$((P-1)); N=$((N+1));; *) break;; esac; done; if [ "
                "\"$N\" -gt 0 ] || [ \"$P\" -eq \"$S\" ]; then B=$(tail -c +$P ");
    pd_puts(&b, log);
    pd_puts(&b, " 2>/dev/null | head -c 1 | od -An -to1 | tr -dc '0-9'); case \"$B\" in "
                "3[0-3][0-7]) L=2;; 3[4-5][0-7]) L=3;; 3[6-7][0-7]) L=4;; *) L=1;; esac; "
                "if [ \"$L\" -gt $((N+1)) ]; then S=$((P-1)); fi; fi; esac; echo \"$S $O "
                "$T\"; if [ \"$S\" -gt \"$O\" ]; then tail -c +$((O+1)) ");
    pd_puts(&b, log);
    pd_puts(&b, " 2>/dev/null | head -c $((S-O)); fi");
    if (b.oom) { free(b.buf); return NULL; }
    out = b.buf;
    return out;
}

/* ---------- parse ---------- */

static int pd_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* CPython int() over an ASCII digit run of at most `max_digits`; 0 on failure. */
static int pd_digits_to_ll(const char *s, size_t n, long long *out) {
    unsigned long long acc = 0;
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        unsigned d;
        if (s[i] < '0' || s[i] > '9') return 0;
        d = (unsigned)(s[i] - '0');
        if (acc > (9223372036854775807ULL - d) / 10ULL) return 0;
        acc = acc * 10ULL + d;
    }
    *out = (long long)acc;
    return 1;
}

/* `([0-9]{1,20}) ([0-9]{1,20}) (?:(run)|exit:([0-9]{0,9}))` fullmatched. */
static int pd_match_header(const char *s, size_t n, long long *size, long long *offset,
                           int *running, int *has_code, long long *code) {
    size_t i = 0, start;
    start = i;
    while (i < n && i - start < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i == start || !pd_digits_to_ll(s + start, i - start, size)) return 0;
    if (i >= n || s[i] != ' ') return 0;
    i++;
    start = i;
    while (i < n && i - start < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i == start || !pd_digits_to_ll(s + start, i - start, offset)) return 0;
    if (i >= n || s[i] != ' ') return 0;
    i++;
    if (n - i == 3 && !strncmp(s + i, "run", 3)) {
        *running = 1;
        *has_code = 0;
        *code = 0;
        return 1;
    }
    if (n - i >= 5 && !strncmp(s + i, "exit:", 5)) {
        size_t dstart = i + 5;
        size_t dcount = n - dstart;
        if (dcount > 9) return 0;
        if (dcount == 0) {
            *running = 0;
            *has_code = 0;
            *code = -1;
            return 1;
        }
        if (!pd_digits_to_ll(s + dstart, dcount, code)) return 0;
        *running = 0;
        *has_code = 1;
        return 1;
    }
    return 0;
}

json_t *w22_processreg_cd9f60_parse_log_delta_reply(const char *raw) {
    const char *text = raw ? raw : "";
    const char *nl = strchr(text, '\n');
    size_t hn = nl ? (size_t)(nl - text) : strlen(text);
    const char *delta = nl ? nl + 1 : "";
    const char *h = text;
    size_t n = hn;
    long long size = 0, offset = 0, code = 0;
    int running = 0, has_code = 0;
    json_t *out;

    while (n > 0 && pd_is_space(h[n - 1])) n--;
    while (n > 0 && pd_is_space(*h)) { h++; n--; }
    if (!pd_match_header(h, n, &size, &offset, &running, &has_code, &code)) return json_null();

    out = json_object();
    json_set(out, "size", json_number((double)size));
    json_set(out, "offset", json_number((double)offset));
    if (running) json_set(out, "exit_code", json_null());
    else json_set(out, "exit_code", json_number((double)(has_code ? code : -1)));
    json_set(out, "delta", json_string(delta));
    return out;
}

/* ---------- fallback_exit_code ---------- */

static int pd_is_line_break(char c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == '\034' || c == '\035' ||
           c == '\036';
}

/* `s.splitlines()[-1]`: the last line, a CRLF counted as one break and a
 * trailing break adding no empty element. 0 when the text has no lines at all
 * (`"".splitlines() == []`). */
static int pd_last_line(const char *s, const char **out, size_t *out_len) {
    size_t n = strlen(s), i = 0, start = 0;
    size_t last_start = 0, last_end = 0;
    int have = 0;
    while (i < n) {
        if (pd_is_line_break(s[i])) {
            last_start = start;
            last_end = i;
            have = 1;
            if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
            start = i;
            if (i >= n) {
                *out = s + last_start;
                *out_len = last_end - last_start;
                return 1;
            }
            continue;
        }
        i++;
    }
    if (i > start) { last_start = start; last_end = n; have = 1; }
    if (!have) return 0;
    *out = s + last_start;
    *out_len = last_end - last_start;
    return 1;
}

/* `int(text.strip())` with ValueError -> not parseable. */
static int pd_py_int_strict(const char *s, size_t n, long long *out) {
    const char *p = s;
    size_t i = 0;
    int neg = 0, digits = 0;
    unsigned long long acc = 0;
    while (n > 0 && pd_is_space(p[n - 1])) n--;
    while (n > 0 && pd_is_space(*p)) { p++; n--; }
    if (n == 0) return 0;
    if (p[0] == '+' || p[0] == '-') { neg = (p[0] == '-'); i = 1; }
    for (; i < n; i++) {
        char c = p[i];
        if (c == '_') {
            if (digits == 0 || i + 1 >= n || p[i - 1] < '0' || p[i - 1] > '9' ||
                p[i + 1] < '0' || p[i + 1] > '9') return 0;
            continue;
        }
        if (c < '0' || c > '9') return 0;
        if (acc > (9223372036854775807ULL - (unsigned)(c - '0')) / 10ULL) return 0;
        acc = acc * 10ULL + (unsigned)(c - '0');
        digits++;
    }
    if (digits == 0) return 0;
    if (neg) {
        if (acc > 9223372036854775808ULL) return 0;
        *out = (long long)(-(long long)(acc - 1) - 1);
    } else {
        *out = (long long)acc;
    }
    return 1;
}

json_t *w22_processreg_cd9f60_fallback_exit_code(const char *check_output,
                                                 const char *exit_output) {
    const char *text = check_output ? check_output : "";
    const char *line = NULL;
    size_t ln = 0, start, n;
    long long code = 0;

    /* `check_output.strip()` */
    start = 0;
    n = strlen(text);
    while (n > 0 && pd_is_space(text[n - 1])) n--;
    while (n > 0 && pd_is_space(text[start])) { start++; n--; }
    if (n == 0) return json_null();                      /* not check_output */
    if (pd_last_line(text + start, &line, &ln)) {
        size_t a = 0, m = ln;
        while (m > 0 && pd_is_space(line[m - 1])) m--;
        while (m > 0 && pd_is_space(line[a])) { a++; m--; }
        if (m == 1 && line[a] == '0') return json_null(); /* still alive */
    }

    {
        const char *etext = exit_output ? exit_output : "";
        size_t es = 0, en = strlen(etext);
        const char *eline = NULL;
        size_t eln = 0;
        while (en > 0 && pd_is_space(etext[en - 1])) en--;
        while (en > 0 && pd_is_space(etext[es])) { es++; en--; }
        if (!pd_last_line(etext + es, &eline, &eln)) return json_number(-1.0);
        if (!pd_py_int_strict(eline, eln, &code)) return json_number(-1.0);
    }
    return json_number((double)code);
}
