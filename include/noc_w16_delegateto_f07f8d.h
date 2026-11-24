#ifndef NOC_W16_DELEGATETO_F07F8D_H
#define NOC_W16_DELEGATETO_F07F8D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_delegateto_f07f8d_refusal @ tools/delegate_tool_continue.py:_refusal */
struct json_t *w16_delegateto_f07f8d_refusal(const struct json_t *opts);
/* PoP: w16_delegateto_f07f8d_claim_resume @ tools/delegate_tool_continue.py:_claim_resume */
struct json_t *w16_delegateto_f07f8d_claim_resume(const struct json_t *state);
/* PoP: w16_delegateto_f07f8d_validate @ tools/delegate_tool_continue.py:continue_subagent */
struct json_t *w16_delegateto_f07f8d_validate(const struct json_t *args);
/* PoP: w16_delegateto_f07f8d_not_found @ tools/delegate_tool_continue.py:_NOT_FOUND */
struct json_t *w16_delegateto_f07f8d_not_found(const char *sid);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_DELEGATETO_F07F8D_H */
