/* Slermes C port — agent/redact.py (pure helper subset)
 *
 * Faithful port of two prefix-based redaction helpers. No live/runtime deps.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
