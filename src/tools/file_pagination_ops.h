#ifndef SLERMES_FILE_PAGINATION_OPS_H
#define SLERMES_FILE_PAGINATION_OPS_H

#include <stdbool.h>
#include "hermes_json.h"

/*
 * file_pagination_ops — read/search pagination clamping + newline-regex
 * detection helpers, ported from tools/file_operations.py. Focused module
 * (extracted from port_file_operations.c in the v554 monolith split).
 *
 * Public API (each has a /* PoP: c @ tools/file_operations.py:_py *​/ in .c):
 *   file_pagination_ops_normalize_read_pagination
 *   file_pagination_ops_normalize_search_pagination
 *   file_pagination_ops_is_line_oriented_newline_error
 *   file_pagination_ops_pattern_has_regex_newline
 *   file_pagination_ops_maybe_warn_line_oriented_newline_pattern
 *
 * Python constants mirrored here for faithful clamping (tools/file_operations.py:681):
 *   MAX_LINES = 2000, DEFAULT_READ_OFFSET = 1, DEFAULT_READ_LIMIT = 500,
 *   DEFAULT_SEARCH_OFFSET = 0, DEFAULT_SEARCH_LIMIT = 50.
 */

/* PoP: file_pagination_ops_normalize_read_pagination @ tools/file_operations.py:normalize_read_pagination */
char *file_pagination_ops_normalize_read_pagination(int offset, int limit, int default_limit);

/* PoP: file_pagination_ops_normalize_search_pagination @ tools/file_operations.py:normalize_search_pagination */
char *file_pagination_ops_normalize_search_pagination(int offset, int limit, int default_limit);

/* PoP: file_pagination_ops_is_line_oriented_newline_error @ tools/file_operations.py:_is_line_oriented_newline_error */
bool file_pagination_ops_is_line_oriented_newline_error(const char *error);

/* PoP: file_pagination_ops_pattern_has_regex_newline @ tools/file_operations.py:_pattern_has_regex_newline */
bool file_pagination_ops_pattern_has_regex_newline(const char *pattern);

/* PoP: file_pagination_ops_maybe_warn_line_oriented_newline_pattern @ tools/file_operations.py:_maybe_warn_line_oriented_newline_pattern */
/* Mutates result: only when total_count==0 AND pattern has a regex newline AND */
/* (no error OR error is the line-oriented error), it clears error and sets the */
/* warning string. Returns result unchanged otherwise. */
json_t *file_pagination_ops_maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern);

#endif /* SLERMES_FILE_PAGINATION_OPS_H */
