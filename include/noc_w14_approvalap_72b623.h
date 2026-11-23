#ifndef NOC_W14_APPROVALAP_72B623_H
#define NOC_W14_APPROVALAP_72B623_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_approvalap_72b623_mentions_osascript @ tools/approval_applescript.py:_mentions_osascript */
struct json_t *w14_approvalap_72b623_mentions_osascript(const struct json_t *text);
/* PoP: w14_approvalap_72b623_strip_layout @ tools/approval_applescript.py:_strip_layout */
struct json_t *w14_approvalap_72b623_strip_layout(const struct json_t *program);
/* PoP: w14_approvalap_72b623_heredoc_body @ tools/approval_applescript.py:_heredoc_body */
struct json_t *w14_approvalap_72b623_heredoc_body(const struct json_t *text, const struct json_t *after,
        const struct json_t *delim, const struct json_t *strip_tabs);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_APPROVALAP_72B623_H */
