#ifndef NOC_W14_APPROVALGI_B1C8FF_H
#define NOC_W14_APPROVALGI_B1C8FF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_approvalgi_b1c8ff_join_escaped_word_chars @ tools/approval_git_options.py:_join_escaped_word_chars
 * Turn each escaped separator/space/tab/quote (outside single quotes: also
 * space, tab, quote, backtick) into `_`, keeping it literal after normalization. */
struct json_t *w14_approvalgi_b1c8ff_join_escaped_word_chars(const char *command);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_APPROVALGI_B1C8FF_H */
