#ifndef NOC_W15_APPROVALGA_206913_H
#define NOC_W15_APPROVALGA_206913_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_approvalga_206913_identity_digest @ tools/approval_gateway_wait.py:_identity_digest
 * sha256(str(text).encode("utf-8","surrogatepass")).hexdigest(); NULL text means
 * python None -> sha256("None"). Returns a malloc'd 64-char hex string. */
char *w15_approvalga_206913_identity_digest(const char *text);

/* PoP: w15_approvalga_206913_same_answerers @ tools/approval_gateway_wait.py:_same_answerers */
int w15_approvalga_206913_same_answerers(const struct json_t *leader,
                                         const struct json_t *follower);

/* PoP: w15_approvalga_206913_hook_choice @ tools/approval_gateway_wait.py:_hook_choice
 * Returns a malloc'd string (caller frees). */
char *w15_approvalga_206913_hook_choice(int resolved, const struct json_t *choice,
                                        const struct json_t *extra);

/* PoP: w15_approvalga_206913_cancel_cause @ tools/approval_gateway_wait.py:_cancel_cause
 * `interrupt_reason` is an explicit seam for get_interrupt_reason() (JSON null
 * stands in for None). Returns a malloc'd string, or NULL for python None. */
char *w15_approvalga_206913_cancel_cause(const char *state, const struct json_t *entry,
                                         const char *interrupt_reason);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_APPROVALGA_206913_H */
