/*
 * browser_redact.c — faithful C port of agent/redact.py (redact_sensitive_text
 * + redact_cdp_url), extracted as the redaction backend for the browser
 * supervisor split (v555).
 *
 * Faithful to tools/browser_supervisor.py:_redact_cdp_error_text /
 * _redact_supervisor_text, which delegate to agent.redact. Covers:
 *   - known vendor secret prefixes (_PREFIX_PATTERNS) — PARTIAL mask
 *   - Authorization / Bearer / x-api-key headers — FULL mask
 *   - private key blocks, DB connection-string passwords — FULL mask
 *   - Telegram bot tokens, bare-token URL userinfo — FULL mask
 *   - CDP URL redaction: sensitive query params + user:pass@ userinfo
 *
 * Masking: PREFIX pass uses _mask_token (head6/tail4, 18-char floor). All other
 * passes FULL-mask to "***", matching the live agent.redact net behavior.
 *
 * Uses POSIX ERE (<regex.h>). NOTES on portability gotchas discovered here:
 *   - (?:...) NON-capturing groups FAIL to compile under this glibc; use (...).
 *   - Negated classes with POSIX classes ([^[:space:]]) FAIL; use literal [^ \t].
 *   - Python lookbehind/lookahead boundaries emulated by char checks.
 * A redact_subst() helper applies a substitution template with \1..\n backrefs
 * plus a \M sentinel for the masked group, mirroring Python's re.sub lambdas.
 */

#include "browser_redact.h"
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- token masking (mirrors agent.redact._mask_token -> mask_secret) ---- */
/* PoP: _mask_token @ agent/redact.py:_mask_token */
static char *mask_token(const char *tok)
{
    if (!tok || !*tok) return strdup("***");
    size_t n = strlen(tok);
    if (n < 18) return strdup("***");          /* below floor -> fully masked */
    /* preserve 6 prefix / 4 suffix: head...tail */
    char *out = malloc(6 + 3 + 4 + 1);
    if (!out) return strdup("***");
    memcpy(out, tok, 6);
    out[6] = '.'; out[7] = '.'; out[8] = '.';
    memcpy(out + 9, tok + n - 4, 4);
    out[13] = '\0';
    return out;
}

/* Full mask (used for non-prefix passes). */
static char *mask_full(const char *tok)
{
    (void)tok;
    return strdup("***");
}

static char *append_str(char *buf, size_t *cap, size_t *len, const char *s, size_t slen)
{
    if (*len + slen + 1 > *cap) {
        while (*len + slen + 1 > *cap) *cap *= 2;
        char *nb = realloc(buf, *cap);
        if (!nb) return buf;
        buf = nb;
    }
    memcpy(buf + *len, s, slen);
    *len += slen;
    buf[*len] = '\0';
    return buf;
}

/*
 * Apply regex `pattern` to `text`, replacing each match per `template`.
 * Template syntax: literal text, \1..\9 -> capture group, \M -> mask(group mg).
 * mg is the 1-based group to mask. Returns newly-allocated string.
 */
static char *redact_subst(const char *pattern, const char *text, const char *templ,
                           int mg, char *(*mask)(const char *))
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE) != 0) return strdup(text);
    size_t cap = strlen(text) + 1, len = 0;
    char *out = malloc(cap);
    if (!out) { regfree(&re); return NULL; }
    out[0] = '\0';

    const char *cur = text;
    regmatch_t pm[16];
    while (regexec(&re, cur, 16, pm, 0) == 0) {
        int so = pm[0].rm_so, eo = pm[0].rm_eo;
        if (so == eo) break;
        append_str(out, &cap, &len, cur, (size_t)so);
        /* expand template */
        for (const char *p = templ; *p; p++) {
            if (*p == '\\' && p[1]) {
                char c = p[1]; p++;
                if (c >= '1' && c <= '9') {
                    int gi = c - '0';
                    if (pm[gi].rm_so >= 0) {
                        int gs = pm[gi].rm_so - so, ge = pm[gi].rm_eo - so;
                        append_str(out, &cap, &len, cur + so + gs, (size_t)(ge - gs));
                    }
                } else if (c == 'M') {
                    if (pm[mg].rm_so >= 0) {
                        int gs = pm[mg].rm_so - so, ge = pm[mg].rm_eo - so;
                        char *gt = strndup(cur + so + gs, ge - gs);
                        char *m = mask ? mask(gt) : strdup("***");
                        append_str(out, &cap, &len, m, strlen(m));
                        free(m); free(gt);
                    }
                } else {
                    append_str(out, &cap, &len, &c, 1);
                }
            } else {
                append_str(out, &cap, &len, p, 1);
            }
        }
        cur += eo;
    }
    append_str(out, &cap, &len, cur, strlen(cur));
    regfree(&re);
    return out;
}

static char *replace_all_literal(const char *pattern, const char *text, const char *repl)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE) != 0) return strdup(text);
    size_t cap = strlen(text) + strlen(repl) + 1, len = 0;
    char *out = malloc(cap);
    if (!out) { regfree(&re); return NULL; }
    out[0] = '\0';
    const char *cur = text;
    regmatch_t pm[1];
    while (regexec(&re, cur, 1, pm, 0) == 0) {
        int so = pm[0].rm_so, eo = pm[0].rm_eo;
        if (so == eo) break;
        append_str(out, &cap, &len, cur, (size_t)so);
        append_str(out, &cap, &len, repl, strlen(repl));
        cur += eo;
    }
    append_str(out, &cap, &len, cur, strlen(cur));
    regfree(&re);
    return out;
}

static char *redact_telegram(const char *text)
{
    regex_t re;
    const char *pat = "(bot)?([0-9]{8,}):([-A-Za-z0-9_]{30,})";
    if (regcomp(&re, pat, REG_EXTENDED | REG_ICASE) != 0) return strdup(text);
    size_t cap = strlen(text) + 8, len = 0;
    char *out = malloc(cap);
    if (!out) { regfree(&re); return NULL; }
    out[0] = '\0';
    const char *cur = text;
    regmatch_t pm[4];
    while (regexec(&re, cur, 4, pm, 0) == 0) {
        int so = pm[0].rm_so, eo = pm[0].rm_eo;
        if (so == eo) break;
        append_str(out, &cap, &len, cur, (size_t)so);
        size_t g1s = pm[1].rm_so >= 0 ? (size_t)(pm[1].rm_so - so) : 0;
        size_t g1e = pm[1].rm_so >= 0 ? (size_t)(pm[1].rm_eo - so) : 0;
        size_t g2s = (size_t)(pm[2].rm_so - so), g2e = (size_t)(pm[2].rm_eo - so);
        if (g1e > g1s) append_str(out, &cap, &len, cur + so + g1s, g1e - g1s);
        append_str(out, &cap, &len, cur + so + g2s, g2e - g2s);
        append_str(out, &cap, &len, ":***", 4);
        cur += eo;
    }
    append_str(out, &cap, &len, cur, strlen(cur));
    regfree(&re);
    return out;
}

static const char *SENSITIVE_QUERY_PARAMS[] = {
    "access_token", "refresh_token", "id_token", "token", "api_key", "apikey",
    "client_secret", "password", "auth", "jwt", "session", "secret", "key",
    "code", "signature", "x-amz-signature", NULL
};

static char *redact_url_query_params(const char *text)
{
    regex_t re;
    const char *pat = "(https?|wss?|ftp)://([^ \t/?#]+)([^ \t?#]*)\\?([^ \t#]+)(#[^ \t]*)?";
    if (regcomp(&re, pat, REG_EXTENDED | REG_ICASE) != 0) return strdup(text);
    size_t cap = strlen(text) + 64, len = 0;
    char *out = malloc(cap);
    if (!out) { regfree(&re); return NULL; }
    out[0] = '\0';
    const char *cur = text;
    regmatch_t pm[6];
    while (regexec(&re, cur, 6, pm, 0) == 0) {
        int so = pm[0].rm_so, eo = pm[0].rm_eo;
        if (so == eo) break;
        size_t g3e = (size_t)(pm[3].rm_eo - so);
        append_str(out, &cap, &len, cur, (size_t)(g3e));
        size_t qs = (size_t)(pm[4].rm_so - so), qe = (size_t)(pm[4].rm_eo - so);
        const char *q = cur + so + qs;
        size_t qlen = qe - qs;
        append_str(out, &cap, &len, "?", 1);
        const char *p = q, *pend = q + qlen;
        int first = 1;
        while (p < pend) {
            const char *amp = memchr(p, '&', (size_t)(pend - p));
            size_t plen = amp ? (size_t)(amp - p) : (size_t)(pend - p);
            const char *eq = memchr(p, '=', plen);
            if (!first) append_str(out, &cap, &len, "&", 1);
            first = 0;
            if (eq) {
                size_t klen = (size_t)(eq - p);
                char *key = strndup(p, klen);
                int sens = 0;
                for (int i = 0; SENSITIVE_QUERY_PARAMS[i]; i++)
                    if (strcasecmp(key, SENSITIVE_QUERY_PARAMS[i]) == 0) { sens = 1; break; }
                free(key);
                append_str(out, &cap, &len, p, klen + 1);
                if (sens) append_str(out, &cap, &len, "***", 3);
                else append_str(out, &cap, &len, eq + 1, plen - klen - 1);
            } else {
                append_str(out, &cap, &len, p, plen);
            }
            p = amp ? amp + 1 : pend;
        }
        if (pm[5].rm_so >= 0)
            append_str(out, &cap, &len, cur + so + (size_t)(pm[5].rm_so - so),
                       (size_t)(pm[5].rm_eo - pm[5].rm_so));
        cur += eo;
    }
    append_str(out, &cap, &len, cur, strlen(cur));
    regfree(&re);
    return out;
}

/* ---- public API ---- */

char *browser_redact_sensitive_text(const char *value)
{
    if (!value) return NULL;
    if (!*value) return strdup("");
    char *t = strdup(value);
    if (!t) return NULL;

    /* Known vendor prefixes — PARTIAL mask (head6/tail4), boundary-checked. */
    const char *prefix_pat =
        "(sk-[A-Za-z0-9_-]{10,}|ghp_[A-Za-z0-9]{10,}|github_pat_[A-Za-z0-9_]{10,}|"
        "gh[aousr]_[A-Za-z0-9]{10,}|xapp-[0-9]+-[A-Za-z0-9-]{10,}|xox[baprs]-[A-Za-z0-9-]{10,}|"
        "AIza[A-Za-z0-9_-]{30,}|pplx-[A-Za-z0-9]{10,}|fal_[A-Za-z0-9_-]{10,}|fc-[A-Za-z0-9]{10,}|"
        "bb_live_[A-Za-z0-9_-]{10,}|gAAAA[A-Za-z0-9_=-]{20,}|AKIA[A-Z0-9]{16}|"
        "sk_live_[A-Za-z0-9]{10,}|sk_test_[A-Za-z0-9]{10,}|rk_live_[A-Za-z0-9]{10,}|"
        "SG\\.[A-Za-z0-9_-]{10,}|hf_[A-Za-z0-9]{10,}|r8_[A-Za-z0-9]{10,}|npm_[A-Za-z0-9]{10,}|"
        "pypi-[A-Za-z0-9_-]{10,}|dop_v1_[A-Za-z0-9]{10,}|doo_v1_[A-Za-z0-9]{10,}|"
        "am_[A-Za-z0-9_-]{10,}|sk_[A-Za-z0-9_]{10,}|tvly-[A-Za-z0-9]{10,}|exa_[A-Za-z0-9]{10,}|"
        "gsk_[A-Za-z0-9]{10,}|syt_[A-Za-z0-9]{10,}|retaindb_[A-Za-z0-9]{10,}|hsk-[A-Za-z0-9]{10,}|"
        "mem0_[A-Za-z0-9]{10,}|brv_[A-Za-z0-9]{10,}|xai-[A-Za-z0-9]{30,}|ntn_[A-Za-z0-9]{10,}|"
        "fw_[A-Za-z0-9]{30,})";
    /* PREFIX: mask whole match with mask_token (preserves head/tail). */
    {
        regex_t re;
        if (regcomp(&re, prefix_pat, REG_EXTENDED | REG_ICASE) == 0) {
            size_t cap = strlen(t) + 1, len = 0;
            char *out = malloc(cap); out[0] = '\0';
            const char *cur = t; regmatch_t pm[16];
            while (regexec(&re, cur, 16, pm, 0) == 0) {
                int so = pm[0].rm_so, eo = pm[0].rm_eo;
                if (so == eo) break;
                /* boundary before/after: not [A-Za-z0-9_-] */
                int skip = 0;
                if (so > 0) { char c = cur[so-1]; if (isalnum((unsigned char)c)||c=='_'||c=='-') skip=1; }
                if (!skip && cur[eo] != '\0') { char c = cur[eo]; if (isalnum((unsigned char)c)||c=='_'||c=='-') skip=1; }
                if (skip) { append_str(out,&cap,&len,cur,(size_t)(so+1)); cur+=so+1; continue; }
                append_str(out, &cap, &len, cur, (size_t)so);
                size_t tl = (size_t)(eo - so);
                char *gt = strndup(cur + so, tl);
                char *m = mask_token(gt);
                append_str(out, &cap, &len, m, strlen(m));
                free(m); free(gt);
                cur += eo;
            }
            append_str(out, &cap, &len, cur, strlen(cur));
            regfree(&re);
            free(t); t = out;
        }
    }

    /* Authorization: <scheme> <cred> — preserve prefix + scheme word, FULL-mask cred. */
    {
        char *n = redact_subst("((Proxy-)?Authorization:[ \t]*)([A-Za-z0-9]+[ \t]*)?([^ \t\"']+)",
                                t, "\\1\\3\\M", 4, mask_full);
        free(t); t = n;
    }

    /* x-api-key etc: <cred> — preserve header name + ": ", FULL-mask value. */
    {
        char *n = redact_subst("(x-api-key|x-goog-api-key|api-key|apikey|x-api-token|x-auth-token|x-access-token)[ \t]*:[ \t]*([^ \t]+)",
                                t, "\\1: \\M", 2, mask_full);
        free(t); t = n;
    }

    /* Telegram bot tokens */
    { char *n = redact_telegram(t); free(t); t = n; }

    /* Private key blocks */
    { char *n = replace_all_literal("-----BEGIN[A-Z ]*PRIVATE KEY-----[^-]*-----END[A-Z ]*PRIVATE KEY-----", t, "[REDACTED PRIVATE KEY]"); free(t); t = n; }

    /* DB connection strings: scheme://user:PASSWORD@host (FULL-mask password). */
    {
        char *n = redact_subst("(postgres(ql)?|mysql|mongodb(\\+srv)?|redis|amqp)://([^: \t]+):([^@ \t]+)@",
                                t, "\\1://\\4:\\M@", 5, mask_full);
        free(t); t = n;
    }

    /* Bare-token URL userinfo: scheme://TOKEN@host (PARTIAL-mask token). */
    {
        char *n = redact_subst("(https?|wss?|git|ssh|ftp|ftps|sftp)://([^ \t:@/]{8,})@([^ \t]+)",
                                t, "\\1://\\M@\\3", 2, mask_token);
        free(t); t = n;
    }

    /* JWT tokens (eyJ...) — PARTIAL mask (matches live agent.redact net behavior). */
    {
        char *n = redact_subst("eyJ[A-Za-z0-9_-]{10,}(\\.[A-Za-z0-9_=-]{4,}){0,2}",
                                t, "\\M", 0, mask_token);
        free(t); t = n;
    }

    return t;
}

/*
 * PoP: redact_cdp_url @ agent/redact.py:redact_cdp_url
 * Mask secrets in a CDP/browser endpoint URL before it is logged. */
char *browser_redact_cdp_url(const char *value)
{
    if (!value) return NULL;
    if (!*value) return strdup("");
    char *t = browser_redact_sensitive_text(value);
    char *q = redact_url_query_params(t);
    free(t);
    /* user:pass@ in http/ws/ftp URLs (FULL-mask password). */
    char *u = redact_subst("(https?|wss?|ftp)://([^/ \t:@]+):([^/ \t@]+)@", q, "\\1://\\2:\\M@", 3, mask_full);
    free(q);
    return u;
}
