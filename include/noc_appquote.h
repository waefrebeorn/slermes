#ifndef NOC_APPQUOTE_H
#define NOC_APPQUOTE_H
/* C port of tools/approval_quoted_newlines.py (gate: appquote).
 * Returns a malloc'd masked string (caller frees). */
char *appquote_mask_quoted_newlines(const char *command);
char *appquote_mask_quoted_newlines_span(const char *command, long long start, long long end);

#endif /* NOC_APPQUOTE_H */
