#ifndef NOC_W8_APPROVALANS_FC15B6_H
#define NOC_W8_APPROVALANS_FC15B6_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_approvalans_fc15b6_expand @ tools/approval_ansi_c.py:expand_ansi_c_quotes
 * Rewrites shell-recognized $'...' spans to plain single-quoted decoded literals.
 * Returns a malloc'd string; caller frees. */
char *w8_approvalans_fc15b6_expand(const char *command);

/* PoP: w8_approvalans_fc15b6_span_end @ tools/approval_ansi_c.py:_ansi_c_span_end
 * Offset just past the closing quote of the $'...' at `start`, or -1. */
long long w8_approvalans_fc15b6_span_end(const char *text, long long start);

/* PoP: w8_approvalans_fc15b6_is_shell_comment_start @ tools/approval_ansi_c.py:_is_shell_comment_start */
int w8_approvalans_fc15b6_is_shell_comment_start(const char *text, long long index);

/* PoP: w8_approvalans_fc15b6_skip_continuations @ tools/approval_ansi_c.py:_skip_continuations */
long long w8_approvalans_fc15b6_skip_continuations(const char *text, long long i);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_APPROVALANS_FC15B6_H */