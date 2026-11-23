/*
 * scheduler_diagnostics.c — real C11 port of upstream
 * cron/scheduler_diagnostics.py.
 *
 * PoP: scheddiag_external_worker_stderr_tail @ cron/scheduler_diagnostics.py:external_worker_stderr_tail
 * PoP: scheddiag_format_run_error             @ cron/scheduler_diagnostics.py:format_run_error
 * PoP: scheddiag_worker_stderr_tail_chars     @ cron/scheduler_diagnostics.py:_WORKER_STDERR_TAIL_CHARS
 *
 * The upstream module delegates redaction to agent.redact; the C port carries
 * a compact, faithful subset for the redaction shapes it itself introduces
 * (URL userinfo + credential-named query params) so it stays self-contained.
 */
#include "noc_scheddiag.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SD_TAIL_CHARS 1500

typedef struct { char *p; size_t len, cap; } Buf;

static void binit(Buf *b) {
    b->cap = 64; b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void bput(Buf *b, const char *s, size_t n) {
    if (!b->p) return;
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
        if (!b->p) return;
    }
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void bputs(Buf *b, const char *s) { bput(b, s, strlen(s)); }
static char *bdone(Buf *b) { return b->p ? b->p : (char *)calloc(1, 1); }
static char *sd_dup(const char *s) {
    Buf b; binit(&b); bputs(&b, s ? s : ""); return bdone(&b);
}

static bool is_py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *sd_dup_len(const char *s, size_t n);  /* forward decl */
static char *strip_ws(const char *s) {
    size_t a = 0, b;
    if (!s) return sd_dup("");
    b = strlen(s);
    while (a < b && is_py_ws((unsigned char)s[a])) a++;
    while (b > a && is_py_ws((unsigned char)s[b - 1])) b--;
    return sd_dup_len(s + a, b - a);
}
/* helper: duplicate exactly n bytes */
static char *sd_dup_len(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return sd_dup("");
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* UTF-8 code-point count / pointer at a code-point index (invalid bytes
 * count as one, matching Python's errors="replace" well enough for tails). */
static size_t u8_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        n++;
        if (*p < 0x80) p++;
        else if ((*p & 0xE0) == 0xC0) p += (p[1] ? 2 : 1);
        else if ((*p & 0xF0) == 0xE0) p += (p[1] && p[2] ? 3 : 1);
        else if ((*p & 0xF8) == 0xF0) p += (p[1] && p[2] && p[3] ? 4 : 1);
        else p++;
    }
    return n;
}
static const char *u8_at(const char *s, size_t idx) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = 0;
    while (*p) {
        if (n == idx) return (const char *)p;
        n++;
        if (*p < 0x80) p++;
        else if ((*p & 0xE0) == 0xC0) p += (p[1] ? 2 : 1);
        else if ((*p & 0xF0) == 0xE0) p += (p[1] && p[2] ? 3 : 1);
        else if ((*p & 0xF8) == 0xF0) p += (p[1] && p[2] && p[3] ? 4 : 1);
        else p++;
    }
    return (const char *)p;
}

static bool ieq(const char *s, size_t n, const char *lit) {
    size_t m = strlen(lit);
    if (n != m) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)lit[i])) return false;
    return true;
}
static bool sensitive_param(const char *s, size_t n) {
    static const char *const names[] = {
        "access_token", "refresh_token", "id_token", "token", "api_key", "apikey",
        "client_secret", "password", "auth", "jwt", "session", "secret", "key",
        "code", "signature", "x-amz-signature", "x-goog-signature", "sig",
        "usertoken", NULL
    };
    for (int i = 0; names[i]; i++) if (ieq(s, n, names[i])) return true;
    return false;
}

/* redact_url_userinfo: scheme://user:pass@host -> scheme://***:***@host */
static char *redact_url_userinfo(const char *t) {
    Buf out; size_t n = strlen(t), i = 0;
    binit(&out);
    while (i < n) {
        if (i + 2 < n && t[i] == ':' && t[i + 1] == '/' && t[i + 2] == '/') {
            size_t j;
            size_t at = (size_t)-1;
            bool colon = false;
            bputs(&out, "://"); i += 3;
            j = i;
            while (j < n && t[j] != '/' && t[j] != '?' && t[j] != '#' &&
                   !is_py_ws((unsigned char)t[j])) j++;
            for (size_t k = i; k < j; k++) if (t[k] == '@') { at = k; break; }
            if (at != (size_t)-1) {
                for (size_t k = i; k < at; k++) if (t[k] == ':') { colon = true; break; }
            }
            if (colon) {
                bputs(&out, "***:***@");
                i = at + 1;
            } else {
                bput(&out, t + i, j - i);
                i = j;
            }
        } else {
            bput(&out, t + i, 1);
            i++;
        }
    }
    return bdone(&out);
}

/* redact_url_query_params: ?api_key=SECRET -> ?api_key=*** */
static char *redact_query_params(const char *t) {
    Buf out; size_t n = strlen(t), i = 0;
    binit(&out);
    while (i < n) {
        if (t[i] == '?' || t[i] == '&') {
            size_t ks = i + 1, ke = ks, ve;
            while (ke < n && t[ke] != '=' && t[ke] != '&' && t[ke] != '#' &&
                   !is_py_ws((unsigned char)t[ke])) ke++;
            if (ke < n && t[ke] == '=' && ke > ks && sensitive_param(t + ks, ke - ks)) {
                ve = ke + 1;
                while (ve < n && t[ve] != '&' && t[ve] != '#' &&
                       !is_py_ws((unsigned char)t[ve])) ve++;
                bput(&out, t + i, ke - i + 1);  /* "?key=" / "&key=" */
                bputs(&out, "***");
                i = ve;
                continue;
            }
        }
        bput(&out, t + i, 1);
        i++;
    }
    return bdone(&out);
}

static char *sd_redact(const char *t) {
    char *a = redact_url_userinfo(t);
    char *b = redact_query_params(a);
    free(a);
    return b;
}

/* PoP: scheddiag_worker_stderr_tail_chars @ cron/scheduler_diagnostics.py:_WORKER_STDERR_TAIL_CHARS */
int scheddiag_worker_stderr_tail_chars(void) { return SD_TAIL_CHARS; }

/* PoP: scheddiag_external_worker_stderr_tail @ cron/scheduler_diagnostics.py:external_worker_stderr_tail */
char *scheddiag_external_worker_stderr_tail(const char *text) {
    char *stripped, *tail0, *red, *result;
    Buf out;
    size_t cnt, start;
    stripped = strip_ws(text);
    if (!stripped || !*stripped) { free(stripped); return sd_dup(""); }
    cnt = u8_count(stripped);
    if (cnt > (size_t)SD_TAIL_CHARS) {
        start = cnt - (size_t)SD_TAIL_CHARS;
        tail0 = sd_dup_len(u8_at(stripped, start), strlen(u8_at(stripped, start)));
        binit(&out);
        bputs(&out, "\xe2\x80\xa6");  /* U+2026 ellipsis, UTF-8 */
        bputs(&out, tail0);
        free(tail0);
    } else {
        tail0 = sd_dup(stripped);
        binit(&out);
        bputs(&out, tail0);
        free(tail0);
    }
    free(stripped);
    red = sd_redact(out.p);
    free(out.p);
    binit(&out);
    bputs(&out, "; worker stderr: ");
    bputs(&out, red);
    free(red);
    result = bdone(&out);
    return result;
}

/* PoP: scheddiag_format_run_error @ cron/scheduler_diagnostics.py:format_run_error */
char *scheddiag_format_run_error(const char *traceback_text) {
    Buf out;
    char *red = sd_redact(traceback_text);
    binit(&out);
    bputs(&out, "## Error\n\n```\n");
    bputs(&out, red);
    bputs(&out, "\n```\n");
    free(red);
    return bdone(&out);
}

