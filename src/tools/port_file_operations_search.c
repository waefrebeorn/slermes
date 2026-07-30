/* Slermes C port — tools/file_operations.py (search-diagnostics helpers)
 *
 * Faithful port of _search_stdout_and_limit and _split_tool_diagnostics.
 * Pure string transforms over an ExecuteResult-shaped struct.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

typedef struct {
    int exit_code;
    char *stdout;
    char *stderr;
} file_ops_execute_result_t;

/* _SEARCH_TIMEOUT_MARKER_RE = re.compile(r"\n?\[Command timed out after \d+s\]\s*$") */
static const char *SEARCH_TIMEOUT_MARKER_RE = "\n?\\[Command timed out after [0-9]+s\\][ \t]*$";
/* _SEARCH_OUTPUT_RE = re.compile(r'^([A-Za-z]:)?[^\s:][^\n]*?[:\-]\d|^[^\s:][^\s]*$') */
static const char *SEARCH_OUTPUT_RE = "^([A-Za-z]:)?[^ \t:][^\n]*?[-:][0-9]|^[^ \t:][^ \t]*$";

/* PoP: file_ops_search_search_stdout_and_limit @ tools/file_operations.py:_search_stdout_and_limit */
char *file_ops_search_search_stdout_and_limit(const file_ops_execute_result_t *result, char **out_reason)
{
    static regex_t re; static bool compiled = false;
    if (!compiled) { regcomp(&re, SEARCH_TIMEOUT_MARKER_RE, REG_EXTENDED); compiled = true; }
    if (out_reason) *out_reason = NULL;
    if (!result) return strdup("");
    char *stdout = result->stdout ? result->stdout : "";
    if (result->exit_code == 124) {
        /* strip the timeout marker */
        char *out = strdup(stdout);
        regmatch_t m;
        if (regexec(&re, out, 1, &m, 0) == 0) {
            out[m.rm_so] = '\0';
        }
        if (out_reason) *out_reason = strdup("search_timeout");
        return out;
    }
    return strdup(stdout);
}

/* PoP: file_ops_search_split_tool_diagnostics @ tools/file_operations.py:_split_tool_diagnostics */
void file_ops_search_split_tool_diagnostics(const char *output, char **out_diagnostics, char **out_payload)
{
    regex_t re; regcomp(&re, SEARCH_OUTPUT_RE, REG_EXTENDED);
    size_t dcap = 256, pcap = 256, dlen = 0, plen = 0;
    char *diag = malloc(dcap), *pay = malloc(pcap);
    diag[0] = '\0'; pay[0] = '\0';
    if (!output) { if (out_diagnostics) *out_diagnostics = diag; if (out_payload) *out_payload = pay; regfree(&re); return; }
    char *buf = strdup(output);
    char *line = strtok(buf, "\n");
    while (line) {
        /* skip blank lines */
        char *ls = line; while (*ls == ' ' || *ls == '\t') ls++;
        if (ls[0] == '\0') { line = strtok(NULL, "\n"); continue; }
        bool is_diag_prefix = (strncmp(ls, "rg: ", 4) == 0) || (strncmp(ls, "grep: ", 6) == 0);
        if (is_diag_prefix) {
            size_t l = strlen(line);
            if (dlen + l + 2 >= dcap) { dcap = dlen + l + 256; diag = realloc(diag, dcap); }
            strcat(diag + dlen, line); dlen += l; strcat(diag + dlen, "\n"); dlen += 1;
        } else {
            bool is_payload = (strcmp(line, "--") == 0) || (regexec(&re, line, 0, NULL, 0) == 0);
            char *dst = is_payload ? pay : diag;
            size_t *dstlen = is_payload ? &plen : &dlen;
            size_t *dstcap = is_payload ? &pcap : &dcap;
            size_t l = strlen(line);
            if (*dstlen + l + 2 >= *dstcap) { *dstcap = *dstlen + l + 256; dst = realloc(dst, *dstcap); }
            strcat(dst + *dstlen, line); *dstlen += l; strcat(dst + *dstlen, "\n"); *dstlen += 1;
        }
        line = strtok(NULL, "\n");
    }
    /* trim trailing newline */
    if (dlen > 0 && diag[dlen - 1] == '\n') diag[--dlen] = '\0';
    if (plen > 0 && pay[plen - 1] == '\n') pay[--plen] = '\0';
    if (out_diagnostics) *out_diagnostics = diag; else free(diag);
    if (out_payload) *out_payload = pay; else free(pay);
    free(buf);
    regfree(&re);
}
