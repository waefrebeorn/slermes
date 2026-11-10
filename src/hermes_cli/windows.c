/*
 * port_gateway_windows.c
 *
 * Pure helpers ported from hermes_cli/gateway_windows.py (111 defs).
 *
 *   gw_normalize_path     <- _normalize_windows_path
 *   gw_sanitize_filename  <- _sanitize_filename
 *   gw_quote_vbs          <- _quote_vbs_string
 *   gw_should_fall_back   <- _should_fall_back
 *   gw_is_access_denied   <- _is_access_denied
 *   gw_run_argv           <- _gateway_run_argv
 *   gw_owns_service       <- hermes_owns_windows_service
 *
 * The SCM / schtasks / UAC / registry half stays in Python.
 *
 * A dead `_wait_for_gateway_absent` stub (void* params, no PoP annotation, no
 * callers) used to sit here; it was removed rather than kept as fake parity.
 */

#include "hermes_json.h"
#include "hermes_regex.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===========================================================================
 *  REAL PORTS - pure helpers from hermes_cli/gateway_windows.py (111 defs).
 *
 *  The stub above is a placeholder; everything below is oracle-verified
 *  against the live Python functions.
 * =========================================================================== */

#include <ctype.h>
#include "hermes_regex.h"

/* gateway_windows.py:_ACCESS_DENIED_WORDS (7 locales) */
static const char *const GW_ACCESS_DENIED_WORDS =
    "access is denied|acceso denegado|p\xc5\x99\xedstup byl odep\xc5\x99en|"
    "\xe6\x8b\x92\xe7\xbb\x9d\xe8\xae\xbf\xe9\x97\xae|"
    "\xe6\x8b\x92\xe7\xb5\x95\xe5\xad\x98\xe5\x8f\x96|"
    "\xe3\x82\xa2\xe3\x82\xaf\xe3\x82\xbb\xe3\x82\xb9\xe3\x81\x8c\xe6\x8b\x92\xe5\x90\xa6\xe3\x81\x95\xe3\x82\x8c\xe3\x81\xbe\xe3\x81\x97\xe3\x81\x9f|"
    "\xec\x95\xa1\xec\x84\xb8\xec\x8a\xa4\xea\xb0\x80 \xea\xb1\xb0\xeb\xb6\x80\xeb\x90\x98\xec\x97\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4";

/* PoP: _normalize_windows_path @ hermes_cli/gateway_windows.py:_normalize_windows_path */
char *gw_normalize_path(const char *value)
{
    if (!value) return strdup("");

    const char *s = value;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;

    const char *q = s;
    size_t qn = n;
    while (qn > 0 && *q == '"') { q++; qn--; }

    char *out = (char *)malloc(qn + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < qn; i++) out[i] = (q[i] == '\\') ? '/' : q[i];
    out[qn] = '\0';

    size_t m = qn;
    while (m > 0 && out[m - 1] == '/') m--;
    out[m] = '\0';

    for (size_t i = 0; i < m; i++) out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

/* PoP: _sanitize_filename @ hermes_cli/gateway_windows.py:_sanitize_filename */
char *gw_sanitize_filename(const char *value)
{
    if (!value) return strdup("");
    size_t n = strlen(value);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;

    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
            c == '\\' || c == '|' || c == '?' || c == '*' || c <= 0x1f)
            out[k++] = '_';
        else
            out[k++] = (char)c;
    }
    out[k] = '\0';
    return out;
}

/* PoP: _quote_vbs_string @ hermes_cli/gateway_windows.py:_quote_vbs_string */
char *gw_quote_vbs(const char *value, char **err)
{
    if (err) *err = NULL;
    if (!value) return strdup("\"\"");

    size_t n = strlen(value);
    for (size_t i = 0; i < n; i++) {
        if (value[i] == '\r' || value[i] == '\n') {
            if (err) {
                *err = (char *)malloc(64);
                if (*err) snprintf(*err, 64, "refusing to quote VBScript value containing newline");
            }
            return NULL;
        }
    }

    size_t quotes = 0;
    for (size_t i = 0; i < n; i++) if (value[i] == '"') quotes++;

    char *out = (char *)malloc(n + quotes + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = '"';
    for (size_t i = 0; i < n; i++) {
        if (value[i] == '"') { out[k++] = '"'; out[k++] = '"'; }
        else out[k++] = value[i];
    }
    out[k++] = '"';
    out[k] = '\0';
    return out;
}

/* PoP: _is_access_denied @ hermes_cli/gateway_windows.py:_is_access_denied */
int gw_is_access_denied(const char *detail)
{
    hregex_t *re = regex_compile(GW_ACCESS_DENIED_WORDS, 1 /* REG_ICASE */);
    if (!re) return 0;
    regex_match_t *m = regex_search(re, detail ? detail : "");
    int hit = (m && m->matched);
    if (m) regex_match_free(m);
    regex_free(re);
    return hit;
}

/* PoP: _should_fall_back @ hermes_cli/gateway_windows.py:_should_fall_back */
int gw_should_fall_back(int code, const char *detail)
{
    if (code == 124) return 1;

    char pat[512];
    snprintf(pat, sizeof(pat), "(%s|schtasks timed out|schtasks produced no output)",
             GW_ACCESS_DENIED_WORDS);
    hregex_t *re = regex_compile(pat, 1 /* REG_ICASE */);
    if (!re) return 0;
    regex_match_t *m = regex_search(re, detail ? detail : "");
    int hit = (m && m->matched);
    if (m) regex_match_free(m);
    regex_free(re);
    return hit;
}

/* PoP: _gateway_run_argv @ hermes_cli/gateway_windows.py:_gateway_run_argv */
char **gw_run_argv(const char *python_exe, const char *const *profile_arg,
                   int *out_count)
{
    int np = 0;
    if (profile_arg) while (profile_arg[np]) np++;

    char **argv = (char **)calloc((size_t)np + 6, sizeof(char *));
    if (!argv) { if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    argv[k++] = strdup(python_exe ? python_exe : "");
    argv[k++] = strdup("-m");
    argv[k++] = strdup("hermes_cli.main");
    for (int i = 0; i < np; i++) argv[k++] = strdup(profile_arg[i]);
    argv[k++] = strdup("gateway");
    argv[k++] = strdup("run");

    if (out_count) *out_count = k;
    return argv;
}

/* PoP: hermes_owns_windows_service @ hermes_cli/gateway_windows.py:hermes_owns_windows_service */
int gw_owns_service(const char *name, const char *binpath,
                    const char *const *hermes_roots)
{
    if (name) {
        size_t n = strlen(name);
        char *low = (char *)malloc(n + 1);
        if (!low) return 0;
        for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)name[i]);
        low[n] = '\0';

        char *alnum = (char *)malloc(n + 1);
        if (!alnum) { free(low); return 0; }
        size_t k = 0;
        for (size_t i = 0; i < n; i++)
            if (isalnum((unsigned char)low[i])) alnum[k++] = low[i];
        alnum[k] = '\0';
        int hit = (strncmp(alnum, "hermes", 6) == 0);
        free(low);
        free(alnum);
        if (hit) return 1;
    }

    char *candidate = gw_normalize_path(binpath);
    if (!candidate) return 0;

    int owned = 0;
    for (int i = 0; hermes_roots && hermes_roots[i]; i++) {
        if (!hermes_roots[i][0]) continue;
        char *root = gw_normalize_path(hermes_roots[i]);
        if (!root) continue;
        size_t rl = strlen(root);
        if (strncmp(candidate, root, rl) == 0 && candidate[rl] == '/') {
            owned = 1;
            free(root);
            break;
        }
        free(root);
    }
    free(candidate);
    return owned;
}