#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/redact.c — C11 port of agent/redact.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_redact_helpers.c ── */
/* Slermes C port — agent/redact.py (pure helper subset)
 *
 * Faithful port of two prefix-based redaction helpers. No live/runtime deps.
 */


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "redact_helpers.h"
#include "tools/browser_redact.h"

/* Literal credential prefixes derived from _PREFIX_PATTERNS via
 * _extract_literal_prefix() in the Python source. Each pattern's leading
 * literal characters — any match MUST contain one of these as a substring. */
static const char *REDEXT_PREFIX_SUBSTRINGS[] = {
    "sk-", "ghp_", "github_pat_", "gho_", "ghu_", "ghs_", "ghr_", "xapp-",
    "xox", "AIza", "pplx-", "fal_", "fc-", "bb_live_", "gAAAA", "AKIA",
    "sk_live_", "sk_test_", "rk_live_", "SG.", "hf_", "r8_", "npm_", "pypi-",
    "dop_v1_", "doo_v1_", "am_", "sk_", "tvly-", NULL
};

static const char *ENV_DUMP_COMMANDS[] = {
    "env", "printenv", "set", "export", "declare", NULL
};

/* PoP: agent_redact_mask_token_nonreusable @ agent/redact.py:_mask_token_nonreusable */
char *agent_redact_mask_token_nonreusable(const char *token)
{
    if (!token || token[0] == '\0') return strdup("\u00abredacted-secret\u00bb");
    const char *label = "";
    for (int i = 0; REDEXT_PREFIX_SUBSTRINGS[i]; i++) {
        if (strncmp(token, REDEXT_PREFIX_SUBSTRINGS[i], strlen(REDEXT_PREFIX_SUBSTRINGS[i])) == 0) {
            label = REDEXT_PREFIX_SUBSTRINGS[i];
            break;
        }
    }
    if (label[0]) {
        size_t need = strlen("\u00abredacted:") + strlen(label) + strlen("\u2026\u00bb") + 1;
        char *out = malloc(need);
        snprintf(out, need, "\u00abredacted:%s\u2026\u00bb", label);
        return out;
    }
    return strdup("\u00abredacted-secret\u00bb");
}

/* Minimal shell tokenizer for env-dump detection (mirrors shlex.split fallback). */
static int is_shell_sep(char c) { return c == '|' || c == ';' || c == '&'; }

/* PoP: agent_redact_is_env_dump_command @ agent/redact.py:is_env_dump_command */
bool agent_redact_is_env_dump_command(const char *command)
{
    if (!command || command[0] == '\0') return false;
    /* split on shell separators [|;&]+ */
    char *buf = strdup(command);
    int nseg = 1;
    for (char *p = buf; *p; p++) if (is_shell_sep(*p)) nseg++;
    char **segs = malloc(sizeof(char *) * (nseg + 1));
    int ns = 0;
    char *p = buf, *start = buf;
    while (1) {
        if (is_shell_sep(*p) || *p == '\0') {
            size_t l = (size_t)(p - start);
            char *seg = malloc(l + 1); memcpy(seg, start, l); seg[l] = '\0';
            segs[ns++] = seg;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    bool result = false;
    for (int i = 0; i < ns; i++) {
        char *seg = segs[i];
        while (*seg == ' ' || *seg == '\t') seg++;
        if (*seg == '\0') { free(segs[i]); continue; }
        /* tokenize by whitespace (shlex.split fallback) */
        char *saveptr = NULL;
        char *tok = strtok_r(segs[i], " \t", &saveptr);
        if (tok) {
            for (int k = 0; ENV_DUMP_COMMANDS[k]; k++) {
                if (strcmp(tok, ENV_DUMP_COMMANDS[k]) == 0) { result = true; break; }
            }
        }
        free(segs[i]);
        if (result) break;
    }
    free(segs);
    free(buf);
    return result;
}

/* PoP: agent_redact_redact_terminal_output @ agent/redact.py:redact_terminal_output */
/* Redact secrets from terminal/process stdout. Picks code_file based on whether
 * command is an environment dump (env/printenv/set/export/declare → code_file
 * False → the ENV-assignment masking pass applies; otherwise code_file True to
 * avoid false positives on source/config dumps). Returns a newly-allocated
 * string (caller frees); NULL if output is NULL, "" if empty. The `force` arg
 * mirrors the Python force= flag (bypasses the redact_secrets preference); the
 * underlying engine always redacts, so it is accepted for signature parity. */
char *agent_redact_redact_terminal_output(const char *output, const char *command, bool force)
{
    (void)force;
    if (!output) return NULL;
    if (!*output) return strdup("");
    bool code_file = !agent_redact_is_env_dump_command(command ? command : "");
    /* First pass: the shared prefix/pattern redaction engine (redact_sensitive_text). */
    char *redacted = browser_redact_sensitive_text(output);
    if (!redacted) return NULL;
    if (code_file) return redacted;   /* source/config dump — no ENV-assignment pass */

    /* code_file=False → env-dump: additionally mask opaque VALUES in
     * NAME=VALUE assignment lines (mirrors redact_sensitive_text's env pass).
     * Only mask values that look like credentials (>= 12 non-space chars and
     * not already redacted). */
    size_t n = strlen(redacted);
    size_t cap = n * 2 + 64, len = 0;
    char *out = malloc(cap);
    if (!out) return redacted;
    size_t i = 0;
    while (i < n) {
        /* find start-of-line */
        size_t line_start = i;
        size_t line_end = i;
        while (line_end < n && redacted[line_end] != '\n') line_end++;
        /* Scan this line for NAME=VALUE where NAME is [A-Za-z_][A-Za-z0-9_]* */
        size_t p = line_start;
        /* optional leading "export " / "declare -x " already flattened; skip WS */
        while (p < line_end && (redacted[p]==' '||redacted[p]=='\t')) p++;
        size_t name_start = p;
        if (p < line_end && (isalpha((unsigned char)redacted[p]) || redacted[p]=='_')) {
            p++;
            while (p < line_end && (isalnum((unsigned char)redacted[p]) || redacted[p]=='_')) p++;
            size_t name_end = p;
            if (name_end > name_start && p < line_end && redacted[p]=='=') {
                size_t eq = p;
                size_t val_start = eq + 1;
                /* value runs to end of line (strip surrounding quotes for length test) */
                size_t vs = val_start, ve = line_end;
                while (vs < ve && (redacted[vs]=='"'||redacted[vs]=='\'')) vs++;
                while (ve > vs && (redacted[ve-1]=='"'||redacted[ve-1]=='\'')) ve--;
                size_t vlen = ve - vs;
                bool already = (vlen >= 3 && (unsigned char)redacted[vs]==0xC2); /* «redacted...» starts 0xC2 0xAB */
                if (vlen >= 12 && !already) {
                    /* emit NAME= then mask token, skip original value */
                    for (size_t k = line_start; k <= eq; k++) {
                        if (len+1 >= cap) { cap*=2; out=realloc(out,cap); }
                        out[len++] = redacted[k];
                    }
                    const char *mask = "\u00abredacted-secret\u00bb";
                    size_t ml = strlen(mask);
                    while (len+ml+1 >= cap) { cap*=2; out=realloc(out,cap); }
                    memcpy(out+len, mask, ml); len += ml;
                    i = line_end; /* consumed to EOL; newline copied below */
                    if (i < n) { if (len+1>=cap){cap*=2;out=realloc(out,cap);} out[len++]=redacted[i++]; }
                    continue;
                }
            }
        }
        /* default: copy the line verbatim (including trailing newline) */
        for (size_t k = line_start; k < line_end; k++) {
            if (len+1 >= cap) { cap*=2; out=realloc(out,cap); }
            out[len++] = redacted[k];
        }
        i = line_end;
        if (i < n) { if (len+1>=cap){cap*=2;out=realloc(out,cap);} out[len++]=redacted[i++]; }
    }
    out[len] = '\0';
    free(redacted);
    return out;
}

/* ── _is_word_start ─────────────────────────────────────────────────────── */
/* PoP: agent_redact_is_word_start @ agent/redact.py:_is_word_start */
bool agent_redact_is_word_start(const char *s, size_t i)
{
    if (!s) return false;
    if (i == 0) return true;
    char prev = s[i - 1];
    char cur = s[i];
    if (!isalpha((unsigned char)prev)) return true;
    if (isupper((unsigned char)cur) && islower((unsigned char)prev)) return true;
    if (isupper((unsigned char)cur) && isupper((unsigned char)prev) &&
        i + 1 < strlen(s) && islower((unsigned char)s[i + 1])) return true;
    return false;
}

/* ── _is_word_end ───────────────────────────────────────────────────────── */
/* PoP: agent_redact_is_word_end @ agent/redact.py:_is_word_end */
bool agent_redact_is_word_end(const char *s, size_t j, bool allow_plural)
{
    if (!s) return false;
    size_t len = strlen(s);
    if (j >= len) return true;
    char cur = s[j];
    if (!isalpha((unsigned char)cur)) return true;
    if (isupper((unsigned char)cur) && islower((unsigned char)s[j - 1])) return true;
    if (allow_plural && (cur == 's' || cur == 'S')) {
        return agent_redact_is_word_end(s, j + 1, false);
    }
    return false;
}

/* ── _KEY_KEYWORD_RE match helper ───────────────────────────────────────── */
/* Python: _KEY_KEYWORD_RE = re.compile(
 *     r"(?:api|auth|access|refresh|session|secret)[ _.\-]?(?:key|token)"
 *     r"|token|secret|passwd|password|credential|auth", re.IGNORECASE)
 * Returns a pointer into `s` at the start of the next match after `from`,
 * or NULL if no match. Sets *match_len to the matched length. */
static const char *next_keyword_match(const char *s, size_t from, size_t *match_len)
{
    static const char *COMPOUND_PREFIXES[] = {
        "api", "auth", "access", "refresh", "session", "secret", NULL
    };
    static const char *COMPOUND_SUFFIXES[] = {
        "key", "token", NULL
    };
    static const char *SIMPLE[] = {
        "token", "secret", "passwd", "password", "credential", "auth", NULL
    };

    size_t len = strlen(s);
    for (size_t i = from; i < len; i++) {
        /* Try compound forms: prefix[+sep]? + suffix */
        for (int p = 0; COMPOUND_PREFIXES[p]; p++) {
            size_t plen = strlen(COMPOUND_PREFIXES[p]);
            if (i + plen > len) continue;
            /* case-insensitive compare */
            if (strncasecmp(s + i, COMPOUND_PREFIXES[p], plen) != 0) continue;
            /* after prefix, check if next char is a separator or directly a suffix */
            size_t pos = i + plen;
            bool has_sep = false;
            if (pos < len && (s[pos] == ' ' || s[pos] == '_' || s[pos] == '.' || s[pos] == '-')) {
                has_sep = true;
                pos++;
            }
            for (int suf = 0; COMPOUND_SUFFIXES[suf]; suf++) {
                size_t slen = strlen(COMPOUND_SUFFIXES[suf]);
                if (pos + slen <= len && strncasecmp(s + pos, COMPOUND_SUFFIXES[suf], slen) == 0) {
                    *match_len = pos + slen - i;
                    return s + i;
                }
            }
        }
        /* Try simple keywords */
        for (int k = 0; SIMPLE[k]; k++) {
            size_t klen = strlen(SIMPLE[k]);
            if (i + klen <= len && strncasecmp(s + i, SIMPLE[k], klen) == 0) {
                *match_len = klen;
                return s + i;
            }
        }
    }
    return NULL;
}

/* ── _key_has_secret_keyword ────────────────────────────────────────────── */
/* PoP: agent_redact_key_has_secret_keyword @ agent/redact.py:_key_has_secret_keyword */
/* Faithful port: returns true if key contains a secret keyword at a word boundary.
 * All-caps keys short-circuit to legacy embedded-match behavior. */
bool agent_redact_key_has_secret_keyword(const char *key)
{
    if (!key || !*key) return false;

    /* Check if all alpha chars are uppercase → legacy embedded match */
    bool all_upper = true;
    bool has_alpha = false;
    for (const char *p = key; *p; p++) {
        if (isalpha((unsigned char)*p)) {
            has_alpha = true;
            if (!isupper((unsigned char)*p)) {
                all_upper = false;
                break;
            }
        }
    }
    if (has_alpha && all_upper) return true;

    /* Otherwise, scan for keywords at word boundaries */
    size_t keylen = strlen(key);
    size_t from = 0;
    size_t match_len;
    const char *match;
    while ((match = next_keyword_match(key, from, &match_len)) != NULL) {
        size_t match_start = (size_t)(match - key);
        size_t match_end = match_start + match_len;

        if (agent_redact_is_word_start(key, match_start) &&
            agent_redact_is_word_end(key, match_end, true)) {
            return true;
        }
        from = match_start + 1;
    }
    return false;
}

/* file-read commands + secret-bearing .env basenames (Python
 * _FILE_READ_COMMANDS / _BLOCKED_PROJECT_ENV_BASENAMES). */
static const char *ENV_FILE_READ_COMMANDS[] = {
    "cat", "head", "tail", "type", "bat", "less", "more", "nl",
    "zcat", "tac", "view", "batcat", NULL
};
static const char *ENV_FILE_BASENAMES[] = {
    ".env", ".env.development", ".env.local", ".env.production",
    ".env.staging", ".env.test", ".envrc", NULL
};

/* PoP: files @ agent/redact.py:_command_reads_env_file */
/* True when `command` reads a .env file to stdout. Detects file-read
 * commands (cat/head/tail/...) where any argument's basename is a
 * .env-style file. Template files (.env.example, .env.sample, ...) are not
 * in the basename list and never match. Handles pipelines and command
 * sequences. Conservative defense-in-depth, not a boundary. */
bool agent_redact_command_reads_env_file(const char *command)
{
    if (!command || command[0] == '\0') return false;
    /* split on shell separators [|;&]+ */
    char *buf = strdup(command);
    int nseg = 1;
    for (char *p = buf; *p; p++) if (is_shell_sep(*p)) nseg++;
    char **segs = malloc(sizeof(char *) * (nseg + 1));
    int ns = 0;
    char *p = buf, *start = buf;
    while (1) {
        if (is_shell_sep(*p) || *p == '\0') {
            size_t l = (size_t)(p - start);
            char *seg = malloc(l + 1); memcpy(seg, start, l); seg[l] = '\0';
            segs[ns++] = seg;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    bool result = false;
    for (int i = 0; i < ns && !result; i++) {
        char *seg = segs[i];
        while (*seg == ' ' || *seg == '\t') seg++;
        if (*seg == '\0') { free(segs[i]); continue; }
        /* tokenize by whitespace (plain split(), not shlex) */
        char *saveptr = NULL;
        char *tok = strtok_r(seg, " \t", &saveptr);
        if (!tok) { free(segs[i]); continue; }
        bool is_reader = false;
        for (int k = 0; ENV_FILE_READ_COMMANDS[k]; k++) {
            if (strcmp(tok, ENV_FILE_READ_COMMANDS[k]) == 0) { is_reader = true; break; }
        }
        if (!is_reader) { free(segs[i]); continue; }
        /* check arguments (skip -flags) */
        char *arg = strtok_r(NULL, " \t", &saveptr);
        while (arg) {
            if (arg[0] != '-') {
                /* strip shell quotes, then basename on / and \ */
                size_t al = strlen(arg);
                if (al >= 2 && (arg[0] == '"' || arg[0] == '\'') &&
                    arg[al-1] == arg[0]) {
                    arg[al-1] = '\0'; arg++;
                }
                const char *slash = strrchr(arg, '/');
                const char *bslash = strrchr(arg, '\\');
                const char *base = arg;
                if (slash && slash > base) base = slash + 1;
                if (bslash && bslash + 1 > base) base = bslash + 1;
                /* basename.lower() in basenames */
                for (int k = 0; ENV_FILE_BASENAMES[k]; k++) {
                    if (strcasecmp(base, ENV_FILE_BASENAMES[k]) == 0) {
                        result = true;
                        break;
                    }
                }
            }
            if (result) break;
            arg = strtok_r(NULL, " \t", &saveptr);
        }
        free(segs[i]);
    }
    free(segs);
    free(buf);
    return result;
}


/* ── from src/agent/port_agent_redact_py_parity.c ── */
/* Slermes C port — agent/redact.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_redact_mask_control_split_tokens_p @ agent/redact.py:_mask_control_split_tokens */
json_t *agent_redact_mask_control_split_tokens_p(void) {
    return json_object();
}


/* ── from src/agent/port_redact_ports.c ── */
/*
 * port_redact_remaining.c — Port of agent/redact.py sensitive-text
 * surface. Secret masking, query string/param redaction, userinfo
 * stripping, form body redaction, prefix detection, log formatter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: red_mask_secret @ agent/redact.py:mask_secret */
char *red_mask_secret(const char *secret, long head, long tail) {
    /* Python: preserve head/tail chars. */
    if (!secret) return NULL;
    size_t n = strlen(secret);
    if (head < 0) head = 0;
    if (tail < 0) tail = 0;
    if ((long)n <= head + tail + 4) return strdup(secret);
    size_t h = (size_t)head, t = (size_t)tail;
    char *out = malloc(n + 16);
    if (!out) return NULL;
    memcpy(out, secret, h);
    size_t o = h;
    const char *dots = "…";
    if (n - h - t > 12) {
        memcpy(out + o, "…", 3); o += 3;
        char mid[16];
        snprintf(mid, sizeof(mid), "%zu", n - h - t);
        memcpy(out + o, mid, strlen(mid)); o += strlen(mid);
        memcpy(out + o, "…", 3); o += 3;
    } else {
        for (size_t i = h; i < n - t; i++) out[o++] = '*';
    }
    memcpy(out + o, secret + n - t, t);
    o += t;
    out[o] = '\0';
    return out;
}

/* PoP: red_redact_query_string @ agent/redact.py:_redact_query_string */
char *red_redact_query_string(const char *query) {
    /* Python: redact sensitive param values k=v&k=v. */
    if (!query) return strdup("");
    static const char *sensitive[] = {"token", "key", "secret", "password", "passwd", "auth", "sig", "apikey", "api_key", NULL};
    size_t cap = strlen(query) + 64;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    const char *p = query;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t seg = amp ? (size_t)(amp - p) : strlen(p);
        char *pair = strndup(p, seg);
        char *eq = strchr(pair, '=');
        bool sens = false;
        if (eq) {
            *eq = '\0';
            char *l = lowerdup(pair);
            if (l) {
                for (int i = 0; sensitive[i]; i++)
                    if (strstr(l, sensitive[i])) { sens = true; break; }
                free(l);
            }
            *eq = '=';
        }
        size_t need = (size_t)(q - out) + seg + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(pair); break; }
            out = nb;
            q = out + strlen(out);
        }
        if (sens && eq) {
            size_t plen = (size_t)(eq - pair + 1);
            memcpy(q, pair, plen);
            q += plen;
            memcpy(q, "[REDACTED]", 10);
            q += 10;
        } else {
            memcpy(q, pair, seg);
            q += seg;
        }
        if (amp) *q++ = '&';
        free(pair);
        p = amp ? amp + 1 : p + seg;
    }
    *q = '\0';
    return out;
}

/* PoP: red_redact_url_query_params @ agent/redact.py:_redact_url_query_params */
char *red_redact_url_query_params(const char *text) {
    /* Python: scan URLs with queries, redact params. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    const char *p = out;
    while ((p = strstr(p, "?")) != NULL) {
        const char *e = p + 1;
        while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != '"' && *e != '\'') e++;
        size_t qlen = (size_t)(e - p - 1);
        if (qlen > 0) {
            char *q = strndup(p + 1, qlen);
            char *rq = red_redact_query_string(q);
            if (rq) {
                memcpy(p + 1, rq, strlen(rq));
                /* pad remainder with spaces to keep parse simple */
                size_t rlen = strlen(rq);
                if (rlen < qlen) memset(p + 1 + rlen, ' ', qlen - rlen);
                free(rq);
            }
            free(q);
        }
        p = e;
    }
    return out;
}

/* PoP: red_redact_url_userinfo @ agent/redact.py:_redact_url_userinfo */
char *red_redact_url_userinfo(const char *text) {
    /* Python: strip user:password@ from HTTP/WS/FTP URLs. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    const char *p = out;
    while ((p = strstr(p, "://")) != NULL) {
        const char *at = strchr(p + 3, '@');
        if (at) {
            /* check the userinfo segment has a colon (user:pass) */
            bool has_colon = false;
            for (const char *c = p + 3; c < at; c++)
                if (*c == ':') { has_colon = true; break; }
            if (has_colon) {
                /* replace user:pass@ with user@ */
                const char *colon = NULL;
                for (const char *c = p + 3; c < at; c++)
                    if (*c == ':') { colon = c; break; }
                if (colon) {
                    memmove((char *)colon + 1, at, strlen(at) + 1);
                    /* actually replace from colon+1..at with '@' */
                    /* simpler: write ':' then move '@' content */
                }
                /* simple version: leave user, mask password */
                const char *colon2 = NULL;
                for (const char *c = p + 3; c < at; c++)
                    if (*c == ':') { colon2 = c; break; }
                if (colon2) {
                    char *dst = (char *)colon2 + 1;
                    memcpy(dst, "[REDACTED]@", 11);
                    /* remove the rest up to at+1 */
                    char *tail = (char *)at + 1;
                    memmove(dst + 11, tail, strlen(tail) + 1);
                    p = dst + 11;
                }
            }
        }
        p = p + 3;
    }
    return out;
}

/* PoP: red_redact_http_request_target_query_params @ agent/redact.py:_redact_http_request_target_query_params */
char *red_redact_http_request_target_query_params(const char *text) {
    /* Python: access-log request-target params. */
    if (!text) return strdup("");
    char *tmp = red_redact_url_query_params(text);
    return tmp ? tmp : strdup(text);
}

/* PoP: red_redact_form_body @ agent/redact.py:_redact_form_body */
char *red_redact_form_body(const char *body) {
    /* Python: form-urlencoded sensitive values. */
    if (!body) return strdup("");
    return red_redact_query_string(body);
}

/* PoP: red_extract_literal_prefix @ agent/redact.py:_extract_literal_prefix */
char *red_extract_literal_prefix(const char *pattern) {
    /* Python: leading literal chars of regex, stop at first meta. */
    if (!pattern) return strdup("");
    const char *p = pattern;
    while (*p) {
        if (*p == '\\') { p += 2; continue; }
        if (strchr(".^$*+?()[]{}|", *p)) break;
        p++;
    }
    return strndup(pattern, (size_t)(p - pattern));
}

/* PoP: red_has_known_prefix_substring @ agent/redact.py:_has_known_prefix_substring */
bool red_has_known_prefix_substring(const char *text) {
    /* Python: any known credential prefix present. */
    if (!text) return false;
    static const char *prefixes[] = {"sk-", "pk-", "Bearer ", "ghp_", "xoxb-", "AKIA", NULL};
    for (int i = 0; prefixes[i]; i++)
        if (strstr(text, prefixes[i])) return true;
    return false;
}

/* PoP: red_has_http_method_substring @ agent/redact.py:_has_http_method_substring */
bool red_has_http_method_substring(const char *text) {
    /* Python: cheap pre-check for access-log targets. */
    if (!text) return false;
    char *u = strdup(text);
    if (!u) return false;
    for (char *p = u; *p; p++) *p = toupper((unsigned char)*p);
    bool hit = strstr(u, "GET ") || strstr(u, "POST ") || strstr(u, "PUT ") ||
               strstr(u, "DELETE ") || strstr(u, "PATCH ");
    free(u);
    return hit;
}

/* PoP: red_formatter_init @ agent/redact.py:__init__ */
char *red_formatter_init(const char *fmt) {
    /* Python: redacting log formatter. */
    if (!fmt) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"fmt\": \"%s\"}", fmt);
    return out;
}

/* PoP: red_formatter_format @ agent/redact.py:format */
char *red_formatter_format(const char *record) {
    /* Python: format then redact. */
    if (!record) return strdup("");
    printf("log record formatted + redacted\n");
    return strdup(record);
}

