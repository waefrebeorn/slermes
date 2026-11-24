#ifndef NOC_W16_APPROVALPA_CD9D4D_H
#define NOC_W16_APPROVALPA_CD9D4D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_approvalpa_cd9d4d_substitution_depth @ tools/approval_parser_budget.py:substitution_depth */
struct json_t *w16_approvalpa_cd9d4d_substitution_depth(const char *command);
/* PoP: w16_approvalpa_cd9d4d_scan_shell @ tools/approval_parser_budget.py:_scan_shell */
struct json_t *w16_approvalpa_cd9d4d_scan_shell(const struct json_t *opts);
/* PoP: w16_approvalpa_cd9d4d_dollar_paren_end @ tools/approval_parser_budget.py:_scan_dollar_paren_end */
struct json_t *w16_approvalpa_cd9d4d_dollar_paren_end(const char *command, long long start);
/* PoP: w16_approvalpa_cd9d4d_backtick_end @ tools/approval_parser_budget.py:_scan_backtick_end */
struct json_t *w16_approvalpa_cd9d4d_backtick_end(const char *command, long long start);
/* PoP: w16_approvalpa_cd9d4d_heredoc_bodies @ tools/approval_parser_budget.py:_quoted_heredoc_body_spans */
struct json_t *w16_approvalpa_cd9d4d_heredoc_bodies(const char *command, long long newline,
                                                   const struct json_t *pending);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_APPROVALPA_CD9D4D_H */
