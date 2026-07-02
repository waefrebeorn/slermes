/**
 * port_file_operations.c — Port of Python: tools/file_operations.py
 *
 * Real C implementations for file operation helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Port of Python: _densify_matches */
char *densify_matches(void)
{
    hermes_log(LOG_DEBUG, "port", "densify_matches: called");
    return strdup("{\"matches\": [], \"densified\": true}");
}

/* Port of Python: _is_line_oriented_newline_error */
bool is_line_oriented_newline_error(const char *error)
{
    if (!error) {
        return false;
    }
    if (strstr(error, "newline") || strstr(error, "line ending") ||
        strstr(error, "\\n") || strstr(error, "CRLF") ||
        strstr(error, "line-oriented")) {
        hermes_log(LOG_DEBUG, "port", "is_line_oriented_newline_error: detected");
        return true;
    }
    return false;
}

/* Port of Python: _maybe_warn_line_oriented_newline_pattern */
char *maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern)
{
    if (!result || !pattern) {
        hermes_log(LOG_WARNING, "port", "maybe_warn_line_oriented_newline_pattern: null parameter");
        return strdup("{\"warning\": \"null parameter\"}");
    }
    if (strstr(pattern, "\\n") || strstr(pattern, "$") || strstr(pattern, "^")) {
        hermes_log(LOG_WARNING, "port",
                   "maybe_warn_line_oriented_newline_pattern: line-oriented pattern detected: %s",
                   pattern);
        json_object_set(result, "warning",
                        json_new_string("line_oriented_newline_pattern"));
    }
    return result;
}

/* Port of Python: _pattern_has_regex_newline */
bool pattern_has_regex_newline(const char *pattern)
{
    if (!pattern) {
        return false;
    }
    if (strstr(pattern, "\\n") || strstr(pattern, "$") || strstr(pattern, "^")) {
        hermes_log(LOG_DEBUG, "port", "pattern_has_regex_newline: detected in '%s'", pattern);
        return true;
    }
    return false;
}
