/*
 * hermes_regex.h -- C regex convenience wrapper (J16: Python re port).
 * Provides simple regex match/search/sub with POSIX regex backend.
 */
#ifndef HERMES_REGEX_H
#define HERMES_REGEX_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define REGEX_MAX_GROUPS 16

/* Compile flags (Python re module flags subset portable to POSIX ERE).
 * REGEX_FLAG_ICASE     ~ re.IGNORECASE  -> REG_ICASE
 * REGEX_FLAG_MULTILINE ~ re.MULTILINE   -> REG_NEWLINE (^/$ match at
 *                         newlines; POSIX ties "." not matching \n to this
 *                         same bit, which is the documented divergence —
 *                         POSIX ERE cannot separate the two the way Python
 *                         re separates MULTILINE from DOTALL.)
 */
#define REGEX_FLAG_ICASE     1
#define REGEX_FLAG_MULTILINE 2

typedef struct hregex hregex_t;
typedef struct {
    bool    matched;
    int     start[REGEX_MAX_GROUPS];   /* byte offset of each group (-1 if unset) */
    int     end[REGEX_MAX_GROUPS];     /* byte offset one past group end          */
    char   *groups[REGEX_MAX_GROUPS];  /* malloc'd captured text (NULL if unset)  */
    int     group_count;               /* number of capture groups in pattern     */
} regex_match_t;

/* Named-group support (Python re (?P<name>...) syntax). */
#define REGEX_MAX_NAMES REGEX_MAX_GROUPS

hregex_t *regex_compile(const char *pattern, int flags);
regex_match_t *regex_match(hregex_t *re, const char *str);
regex_match_t *regex_search(hregex_t *re, const char *str);
char *regex_replace(hregex_t *re, const char *str, const char *replacement);

/* PoP: re.sub(pattern, repl, string, count=N). Replaces up to max_count
 * occurrences (max_count <= 0 means all, matching Python's count=0 default).
 * Replacement expands "$1".."$15" style backreferences ($0 = whole match).
 * Dynamic output sizing — no fixed cap. Caller free(). NULL on error. */
char *regex_replace_n(hregex_t *re, const char *str, const char *replacement,
                      int max_count);

/* PoP: re.findall — append every group-0 match (left-to-right,
 * non-overlapping) into out (capacity out_max). Returns match count. */
int regex_findall(hregex_t *re, const char *str, char **out, int out_max);

/* PoP: re.split(pattern, string, maxsplit=N). Splits str by matches;
 * maxsplit < 0 means unlimited (Python default). Captured groups are NOT
 * inserted between fields (matches Python when pattern has no capturing
 * groups; documented simplification). Returns field count; fields are
 * malloc'd, caller frees each and the array (or uses regex_split_free). */
int regex_split(hregex_t *re, const char *str, int maxsplit,
                char **out, int out_max);
void regex_split_free(char **fields, int count);

/* Number of capture groups in the compiled pattern (Python re.groups). */
int regex_group_count(hregex_t *re);

/* Index of named group `name`, or -1 if absent (Python re.groupindex). */
int regex_group_index(hregex_t *re, const char *name);

void regex_free(hregex_t *re);
void regex_match_free(regex_match_t *m);
char *regex_extract(const char *pattern, const char *str, int group);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_REGEX_H */
