#ifndef NOC_W7_APPROVALSMA_CFFB5C_H
#define NOC_W7_APPROVALSMA_CFFB5C_H

#include "hermes_json.h"

/* Public C API for the pure shell-comment/verdict helpers of
 * tools/approval_smart.py (gate w7_approvalsma_cffb5c). apprsmart_ prefix
 * avoids collisions with the other approval_* symbols in this repo.
 *
 * All offsets are 0-based character offsets; fixtures use ASCII so byte and
 * character offsets coincide. */
struct json_t *apprsmart_next_comment(const char *text, long start);
struct json_t *apprsmart_comment_spans(const char *text);
struct json_t *apprsmart_strip_line_comment(const char *line);
struct json_t *apprsmart_strip_shell_comments(const char *command);
struct json_t *apprsmart_has_heredoc(const char *command);
struct json_t *apprsmart_verdict(const char *answer);

#endif /* NOC_W7_APPROVALSMA_CFFB5C_H */
