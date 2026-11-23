/*
 * approval_fetch_exec.c — C home for upstream Hermes tools/approval_fetch_exec.py.
 *
 * PoP: w12_approvalfe_c05a66_base         @ tools/approval_fetch_exec.py:_base
 * PoP: w12_approvalfe_c05a66_fetch_targets @ tools/approval_fetch_exec.py:DownloadThenRunMatcher.search
 *
 * Faithful port of _base and of search()'s step 1: collect, for each curl/wget
 * fetch segment, the map of target basename -> offset where that target ends.
 * The regexes are implemented as direct scanners (no PCRE dependency).
 */
#include "noc_w12_approvalfe_c05a66.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char lowerc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int ieq(const char *s, const char *lit) {
    while (*lit) { if (lowerc(*s) != lowerc(*lit)) return 0; s++; lit++; }
    return 1;
}
static int is_word_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static int is_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static int is_hspace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}
static int is_target_char(char c) {
    if (is_hspace(c) || c == '\n') return 0;
    return !strchr(";&|<>'\"`()", c);
}
static int is_seg_end(char c) { return c == '\n' || c == ';' || c == '&' || c == '|'; }

char *w12_approvalfe_c05a66_base(const char *path) {
    if (!path) path = "";
    size_t n = strlen(path);
    while (n > 0 && path[n - 1] == '/') n--;
    size_t start = 0;
    for (size_t i = n; i > 0; --i)
        if (path[i - 1] == '/') { start = i; break; }
    char *out = (char *)malloc(n - start + 1);
    if (out) { memcpy(out, path + start, n - start); out[n - start] = '\0'; }
    return out;
}

/* \b(?:curl|wget)\b, case-insensitive, first at/after from */
static int find_fetch(const char *s, size_t len, size_t from, size_t *ws, size_t *we) {
    for (size_t i = from; i + 4 <= len; ++i) {
        if (!ieq(s + i, "curl") && !ieq(s + i, "wget")) continue;
        if (i > 0 && is_word_char(s[i - 1])) continue;
        if (i + 4 < len && is_word_char(s[i + 4])) continue;
        *ws = i; *we = i + 4;
        return 1;
    }
    return 0;
}

static int match_opt_literal(const char *s, size_t i, size_t seg_end, const char *lit,
                             size_t *after) {
    size_t n = strlen(lit);
    if (i + n > seg_end) return 0;
    if (!ieq(s + i, lit)) return 0;
    *after = i + n;
    return 1;
}

json_t *w12_approvalfe_c05a66_fetch_targets(const char *command) {
    json_t *targets = json_object();
    if (!command) return targets;
    size_t len = strlen(command);
    size_t pos = 0, ws, we;
    while (find_fetch(command, len, pos, &ws, &we)) {
        size_t seg_end = we;
        while (seg_end < len && !is_seg_end(command[seg_end])) seg_end++;

        size_t scan = we;
        while (scan < seg_end) {
            size_t cap_start = (size_t)-1, cap_end = 0;
            size_t i = scan;

            /* A: \s-[a-z]*o\s*+['"]?([^\s;&|<>'"`()]++) */
            if (is_hspace(command[i]) && i + 1 < seg_end && command[i + 1] == '-') {
                size_t j = i + 2;
                while (j < seg_end && is_letter(command[j])) j++;
                /* [a-z]* is greedy: the engine backtracks to the LAST 'o' in
                 * the option word before trying \s*+ and the capture. */
                size_t p = j;
                while (p > i + 2 && lowerc(command[p - 1]) != 'o') p--;
                if (p > i + 2) {
                    j = p;
                    while (j < seg_end && is_hspace(command[j])) j++;
                    if (j < seg_end && (command[j] == '\'' || command[j] == '"')) j++;
                    size_t k = j;
                    while (k < seg_end && is_target_char(command[k])) k++;
                    if (k > j) { cap_start = j; cap_end = k; }
                }
            }

            /* B: \s--output(?:-document)?(?:=|\s++)['"]?(CAP) */
            if (cap_start == (size_t)-1 && is_hspace(command[i]) &&
                i + 2 < seg_end && command[i + 1] == '-' && command[i + 2] == '-') {
                size_t after = 0;
                if (match_opt_literal(command, i + 1, seg_end, "--output-document", &after) ||
                    match_opt_literal(command, i + 1, seg_end, "--output", &after)) {
                    size_t j = after;
                    if (j < seg_end && command[j] == '=') {
                        j++;
                    } else if (j < seg_end && is_hspace(command[j])) {
                        while (j < seg_end && is_hspace(command[j])) j++;
                    } else {
                        j = 0; /* not a match */
                    }
                    if (j) {
                        if (j < seg_end && (command[j] == '\'' || command[j] == '"')) j++;
                        size_t k = j;
                        while (k < seg_end && is_target_char(command[k])) k++;
                        if (k > j) { cap_start = j; cap_end = k; }
                    }
                }
            }

            /* C: >[>|]?\s*+['"]?(CAP) */
            if (cap_start == (size_t)-1 && command[i] == '>') {
                size_t j = i + 1;
                if (j < seg_end && (command[j] == '>' || command[j] == '|')) j++;
                while (j < seg_end && is_hspace(command[j])) j++;
                if (j < seg_end && (command[j] == '\'' || command[j] == '"')) j++;
                size_t k = j;
                while (k < seg_end && is_target_char(command[k])) k++;
                if (k > j) { cap_start = j; cap_end = k; }
            }

            if (cap_start != (size_t)-1) {
                char *cap = (char *)malloc(cap_end - cap_start + 1);
                if (cap) {
                    memcpy(cap, command + cap_start, cap_end - cap_start);
                    cap[cap_end - cap_start] = '\0';
                    char *b = w12_approvalfe_c05a66_base(cap);
                    if (b && !json_obj_get(targets, b))
                        json_set(targets, b, json_number((double)cap_end));
                    free(b);
                    free(cap);
                }
                scan = cap_end;
            } else {
                scan = i + 1;
            }
        }
        pos = seg_end;
        if (seg_end >= len) break;
        /* advance past the segment terminator so the next fetch search resumes */
        pos = seg_end + 1;
    }
    return targets;
}
