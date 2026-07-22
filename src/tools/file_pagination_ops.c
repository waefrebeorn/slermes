/*
 * file_pagination_ops.c — read/search pagination clamping + newline-regex
 * detection helpers, ported from tools/file_operations.py.
 *
 * Faithful to the Python source:
 *  - normalize_read_pagination:  offset = max(1, offset); limit clamped to
 *       [1, MAX_LINES(2000)] after applying default_limit when limit<=0.
 *       (Python: file_operations.py:698, MAX_LINES=2000 at :681.)
 *  - normalize_search_pagination: offset = max(0, offset); limit = max(1, limit).
 *  - is_line_oriented_newline_error: EXACT Python check
 *       ("literal \"\n\" is not allowed" in error AND "--multiline" in error) —
 *       the old C matched loose keywords (false positives).
 *  - pattern_has_regex_newline: "\n" in pattern OR an odd-backslash \n escape
 *       (regex _REGEX_NEWLINE_ESCAPE_RE). The old C matched bare "$"/"^"
 *       (false positives).
 *  - maybe_warn_line_oriented_newline_pattern: only warns when total_count==0
 *       AND pattern has a regex newline AND (no error OR error is the
 *       line-oriented error); clears error and sets the specific warning string.
 *
 * Oracle-verified against LIVE tools/file_operations.py
 * (see tests/sta_oracle_file_pagination_ops.py).
 */

#include "file_pagination_ops.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tools/file_operations.py:681 */
#define MAX_LINES 2000

static int clamp_read_limit(int limit, int default_limit)
{
    /* Python: _coerce_int(limit, DEFAULT_READ_LIMIT) then
     *   max(1, min(normalized_limit, MAX_LINES)). For an int argument the
     *   default is never used (int() never raises), so a non-positive limit
     *   clamps straight to 1, not to default_limit. */
    (void)default_limit;
    if (limit < 1) return 1;
    if (limit > MAX_LINES) return MAX_LINES;
    return limit;
}

static int clamp_search_limit(int limit, int default_limit)
{
    /* Python: max(1, _coerce_int(limit, DEFAULT_SEARCH_LIMIT)) — no upper cap. */
    (void)default_limit;
    return limit < 1 ? 1 : limit;
}

/* PoP: file_pagination_ops_normalize_read_pagination @ tools/file_operations.py:normalize_read_pagination */
char *file_pagination_ops_normalize_read_pagination(int offset, int limit, int default_limit)
{
    int normalized_offset = offset < 1 ? 1 : offset;          /* max(1, offset) */
    int normalized_limit = clamp_read_limit(limit, default_limit);

    json_t *root = json_object();
    json_set(root, "offset", json_number(normalized_offset));
    json_set(root, "limit", json_number(normalized_limit));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* PoP: file_pagination_ops_normalize_search_pagination @ tools/file_operations.py:normalize_search_pagination */
char *file_pagination_ops_normalize_search_pagination(int offset, int limit, int default_limit)
{
    int normalized_offset = offset < 0 ? 0 : offset;          /* max(0, offset) */
    int normalized_limit = clamp_search_limit(limit, default_limit);

    json_t *root = json_object();
    json_set(root, "offset", json_number(normalized_offset));
    json_set(root, "limit", json_number(normalized_limit));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* PoP: file_pagination_ops_is_line_oriented_newline_error @ tools/file_operations.py:_is_line_oriented_newline_error */
bool file_pagination_ops_is_line_oriented_newline_error(const char *error)
{
    if (!error || !*error) return false;
    /* Python: '"literal "\\n" is not allowed" in error and "--multiline" in error' */
    return strstr(error, "literal \"\\n\" is not allowed") != NULL &&
           strstr(error, "--multiline") != NULL;
}

/* PoP: file_pagination_ops_pattern_has_regex_newline @ tools/file_operations.py:_pattern_has_regex_newline */
/* True when pattern contains a literal newline OR a regex \n escape preceded by
 * an ODD number of backslashes (even backslashes => literal backslash+n). */
bool file_pagination_ops_pattern_has_regex_newline(const char *pattern)
{
    if (!pattern) return false;
    if (strchr(pattern, '\n')) return true;
    /* emulate _REGEX_NEWLINE_ESCAPE_RE = r"(?<!\\)(?:\\\\)*\\n" */
    size_t len = strlen(pattern);
    for (size_t i = 0; i + 1 < len; i++) {
        if (pattern[i] == '\\' && pattern[i + 1] == 'n') {
            /* count backslashes ending at position i (inclusive) */
            size_t bs = 0;
            size_t j = i;
            while (j > 0 && pattern[j - 1] == '\\') { bs++; j--; }
            bs++;  /* pattern[i] itself */
            if (bs % 2 == 1) return true;  /* odd => the \n is an escape */
        }
    }
    return false;
}

/* PoP: file_pagination_ops_maybe_warn_line_oriented_newline_pattern @ tools/file_operations.py:_maybe_warn_line_oriented_newline_pattern */
json_t *file_pagination_ops_maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern)
{
    if (!result || !pattern) return result;

    double total_count = json_get_num(result, "total_count", 0);
    const char *error = json_get_str(result, "error", NULL);

    if (total_count != 0) return result;
    if (!file_pagination_ops_pattern_has_regex_newline(pattern)) return result;
    if (error && !file_pagination_ops_is_line_oriented_newline_error(error)) return result;

    /* warning path: clear error, set the specific warning string */
    json_set(result, "error", json_null());
    json_set(result, "warning",
             json_string("0 results found. Note: search_files content search is "
                         "line-oriented and does not run ripgrep with -U/--multiline, "
                         "so `\\n` in the regex does not match line breaks. Use "
                         "context=N to inspect neighboring lines, or escape as `\\\\n` "
                         "when searching for a literal backslash+n."));
    return result;
}
