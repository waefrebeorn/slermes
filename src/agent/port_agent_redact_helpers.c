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
#include "../tools/browser_redact.h"

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

/* PoP: agent_redact__mask_token_nonreusable @ agent/redact.py:_mask_token_nonreusable */
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
