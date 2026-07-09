/*
 * port_agent_display.c — C port of agent/display.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_display___enter__ @ agent/display.py:__enter__ */

/* Port of Python agent/display.py:__enter__ */
/* Context manager entry for display state. */
int cli_agent_display___enter__(void *display_state)
{
    (void)display_state;
    /* CLI port: display state managed by the CLI itself. */
    return 0;
}

/* PoP: cli_agent_display___exit__ @ agent/display.py:__exit__ */

/* Port of Python agent/display.py:__exit__ */
/* Context manager exit for display state. */
int cli_agent_display___exit__(void *display_state, int exc_type)
{
    (void)display_state;
    (void)exc_type;
    /* CLI port: display state managed by the CLI itself. */
    return 0;
}

/* ===================================================================
 * Shell command summarization helpers — pure string/quoting logic.
 * Port of agent/display.py: _shell_basename, _split_shell_words,
 * _strip_shell_pipe_tail, _split_shell_compound, _clean_shell_segment,
 * _is_shell_boundary_echo.
 * =================================================================== */

/* _SHELL_SILENT_HEADS and _SHELL_PIPE_TAIL_HEADS mirrored as C sets. */
static int _in_set(const char *w, const char *const *set, int n)
{
    for (int i = 0; i < n; i++)
        if (strcmp(w, set[i]) == 0) return 1;
    return 0;
}
static const char *_SHELL_SILENT_HEADS[] = {
    "cd", "pushd", "popd", "export", "set", "unset", "source",
    ".", "true", "false", ":", NULL
};
static const char *_SHELL_PIPE_TAIL_HEADS[] = {
    "head", "tail", "wc", "sort", "uniq", NULL
};

/* _shell_basename(head) -> last path component. Caller frees. */
static char *_shell_basename(const char *head)
{
    if (!head || !*head) return strdup("");
    const char *slash = strrchr(head, '/');
    const char *base = slash ? slash + 1 : head;
    return strdup(base);
}

/* _split_shell_words: split on whitespace, honoring ' and " quotes.
 * Returns malloc'd array of malloc'd strings; *count set; NULL term. */
static char **_cli_split_shell_words(const char *segment, int *count)
{
    /* Two passes: count words, then allocate. Keep simple: tokenize into
     * a dynamic array. */
    char **words = NULL;
    int cap = 0, n = 0;
    const char *p = segment;
    char buf[4096];
    size_t blen = 0;
    int in_word = 0;
    char quote = 0;
    while (*p) {
        char c = *p;
        if (quote) {
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            /* emulate Python: quote ends when ch==quote and prev not backslash */
            /* (we don't track backslash specially beyond not ending) */
            if (c == quote) quote = 0;
            p++;
            in_word = 1;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            if (!in_word) { in_word = 1; blen = 0; }
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            p++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (in_word) {
                buf[blen] = '\0';
                if (n + 1 > cap) {
                    cap = cap ? cap * 2 : 8;
                    words = (char **)realloc(words, cap * sizeof(char *));
                }
                words[n++] = strdup(buf);
                in_word = 0; blen = 0;
            }
            p++;
            continue;
        }
        if (!in_word) { in_word = 1; blen = 0; }
        if (blen + 1 < sizeof(buf)) buf[blen++] = c;
        p++;
    }
    if (in_word) {
        buf[blen] = '\0';
        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 8;
            words = (char **)realloc(words, cap * sizeof(char *));
        }
        words[n++] = strdup(buf);
    }
    if (n + 1 > cap) {
        cap = n + 1;
        words = (char **)realloc(words, cap * sizeof(char *));
    }
    words[n] = NULL;
    *count = n;
    return words;
}

static void _free_words(char **words, int n)
{
    for (int i = 0; i < n; i++) free(words[i]);
    free(words);
}

/* _strip_shell_pipe_tail: drop words from the first "| <tail-head>" on. */
static char *_cli_strip_shell_pipe_tail(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    /* Build output up to (but not including) the "|" whose next word basename
     * is in _SHELL_PIPE_TAIL_HEADS. */
    int cut = n;
    for (int i = 0; i < n; i++) {
        if (strcmp(words[i], "|") == 0) {
            const char *next = (i + 1 < n) ? words[i + 1] : "";
            char *bn = _shell_basename(next);
            int is_tail = _in_set(bn, _SHELL_PIPE_TAIL_HEADS,
                                  (int)(sizeof(_SHELL_PIPE_TAIL_HEADS) / sizeof(char *) - 1));
            free(bn);
            if (is_tail) { cut = i; break; }
        }
    }
    /* join words[0..cut) with spaces */
    size_t out_sz = 1;
    for (int i = 0; i < cut; i++) out_sz += strlen(words[i]) + 1;
    char *out = (char *)malloc(out_sz);
    out[0] = '\0';
    for (int i = 0; i < cut; i++) {
        if (i) strcat(out, " ");
        strcat(out, words[i]);
    }
    /* strip trailing whitespace */
    size_t L = strlen(out);
    while (L > 0 && (out[L - 1] == ' ' || out[L - 1] == '\t')) out[--L] = '\0';
    _free_words(words, n);
    return out;
}

/* _split_shell_compound: split on ; & && || \n into segments (pipe-tail
 * stripped). Returns malloc'd array of malloc'd strings; *count set. */
static char **_cli_split_shell_compound(const char *command, int *count)
{
    char **segs = NULL;
    int cap = 0, n = 0;
    char buf[8192];
    size_t blen = 0;
    char quote = 0;
    size_t L = strlen(command);
    size_t i = 0;
    while (i < L) {
        char c = command[i];
        if (quote) {
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            if (c == quote) quote = 0;
            i++;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            if (blen + 1 < sizeof(buf)) buf[blen++] = c;
            i++;
            continue;
        }
        int op_len = 0;
        if (i + 1 < L && (strncmp(command + i, "&&", 2) == 0 || strncmp(command + i, "||", 2) == 0))
            op_len = 2;
        else if (c == ';' || c == '\n')
            op_len = 1;
        if (op_len) {
            buf[blen] = '\0';
            char *seg = _cli_strip_shell_pipe_tail(buf);
            if (seg && *seg) {
                if (n + 1 > cap) { cap = cap ? cap * 2 : 8; segs = (char **)realloc(segs, cap * sizeof(char *)); }
                segs[n++] = seg;
            } else free(seg);
            blen = 0;
            i += op_len;
            continue;
        }
        if (blen + 1 < sizeof(buf)) buf[blen++] = c;
        i++;
    }
    buf[blen] = '\0';
    char *seg = _cli_strip_shell_pipe_tail(buf);
    if (seg && *seg) {
        if (n + 1 > cap) { cap = n + 1; segs = (char **)realloc(segs, cap * sizeof(char *)); }
        segs[n++] = seg;
    } else free(seg);
    if (n + 1 > cap) { cap = n + 1; segs = (char **)realloc(segs, cap * sizeof(char *)); }
    segs[n] = NULL;
    *count = n;
    return segs;
}

/* _clean_shell_segment: drop redirection tokens (N>, >&, <&, etc.) and
 * bare N> / < operators. Returns malloc'd string. */
static char *_cli_clean_shell_segment(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    char **out = (char **)malloc((n + 1) * sizeof(char *));
    int m = 0;
    int i = 0;
    while (i < n) {
        const char *w = words[i];
        /* ^\d*(?:>>?|<)$  -> skip word and the next (the target) */
        int digits = 0; while (w[digits] >= '0' && w[digits] <= '9') digits++;
        int is_redir1 = (strcmp(w + digits, ">") == 0 || strcmp(w + digits, ">>") == 0 || strcmp(w + digits, "<") == 0);
        if (is_redir1) { i += 2; continue; }
        /* ^\d*(?:>&|<&)\d+$ or ^\d*>&\d+$ -> skip just this word */
        int is_redir2 = 0;
        if (w[digits] == '>' && w[digits + 1] == '&') is_redir2 = 1;
        else if (w[digits] == '<' && w[digits + 1] == '&') is_redir2 = 1;
        else if (w[digits] == '>' && w[digits + 1] == '&') is_redir2 = 1;
        if (is_redir2) { i += 1; continue; }
        out[m++] = strdup(w);
        i += 1;
    }
    out[m] = NULL;
    size_t sz = 1;
    for (int k = 0; k < m; k++) sz += strlen(out[k]) + 1;
    char *res = (char *)malloc(sz);
    res[0] = '\0';
    for (int k = 0; k < m; k++) {
        if (k) strcat(res, " ");
        strcat(res, out[k]);
    }
    size_t R = strlen(res);
    while (R > 0 && (res[R - 1] == ' ' || res[R - 1] == '\t')) res[--R] = '\0';
    for (int k = 0; k < m; k++) free(out[k]);
    free(out);
    _free_words(words, n);
    return res;
}

/* _is_shell_boundary_echo: detects `echo ... --... | _exit= | $? | PIPESTATUS` */
static int _cli_is_shell_boundary_echo(const char *segment)
{
    int n = 0;
    char **words = _cli_split_shell_words(segment, &n);
    char *bn = _shell_basename(n > 0 ? words[0] : "");
    int is_echo = (strcmp(bn, "echo") == 0);
    free(bn);
    if (!is_echo) { _free_words(words, n); return 0; }
    /* rest = " ".join(words[1:]) */
    size_t rsz = 1;
    for (int i = 1; i < n; i++) rsz += strlen(words[i]) + 1;
    char *rest = (char *)malloc(rsz);
    rest[0] = '\0';
    for (int i = 1; i < n; i++) { if (i > 1) strcat(rest, " "); strcat(rest, words[i]); }
    int found = (strstr(rest, "--") != NULL)        /* -{2,} */
             || (strstr(rest, "_exit=") != NULL)
             || (strstr(rest, "$?") != NULL)
             || (strstr(rest, "${") != NULL)
             || (strstr(rest, "PIPESTATUS") != NULL);
    free(rest);
    _free_words(words, n);
    return found;
}

/* ── Public CLI entrypoints (PoP-annotated) ────────────────────────── */

/* PoP: cli_agent_display__split_shell_words @ agent/display.py:_split_shell_words */
char **cli_agent_display__split_shell_words(const char *segment, int *count)
{
    return _cli_split_shell_words(segment, count);
}

/* PoP: cli_agent_display__strip_shell_pipe_tail @ agent/display.py:_strip_shell_pipe_tail */
char *cli_agent_display__strip_shell_pipe_tail(const char *segment)
{
    return _cli_strip_shell_pipe_tail(segment);
}

/* PoP: cli_agent_display__split_shell_compound @ agent/display.py:_split_shell_compound */
char **cli_agent_display__split_shell_compound(const char *command, int *count)
{
    return _cli_split_shell_compound(command, count);
}

/* PoP: cli_agent_display__clean_shell_segment @ agent/display.py:_clean_shell_segment */
char *cli_agent_display__clean_shell_segment(const char *segment)
{
    return _cli_clean_shell_segment(segment);
}

/* PoP: cli_agent_display__is_shell_boundary_echo @ agent/display.py:_is_shell_boundary_echo */
int cli_agent_display__is_shell_boundary_echo(const char *segment)
{
    return _cli_is_shell_boundary_echo(segment);
}
