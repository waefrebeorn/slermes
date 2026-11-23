#ifndef NOC_W15_APPROVALSH_F4ACBF_H
#define NOC_W15_APPROVALSH_F4ACBF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_approvalsh_f4acbf_backtick_end_from @ tools/approval_shell_tokens.py:_backtick_end_from
 * Index of the closing backtick opened at i (escaped backticks skipped), or -1
 * for python None. */
long long w15_approvalsh_f4acbf_backtick_end_from(const char *segment, long long i);

/* PoP: w15_approvalsh_f4acbf_shell_tokens_with_spans @ tools/approval_shell_tokens.py:_shell_tokens_with_spans
 * Returns a NEW JSON array of [value, start, end, quoted] tuples, or NULL for
 * python None (malformed quoting). Caller frees. */
struct json_t *w15_approvalsh_f4acbf_shell_tokens_with_spans(const char *segment, long long start);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_APPROVALSH_F4ACBF_H */
