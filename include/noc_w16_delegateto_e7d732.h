#ifndef NOC_W16_DELEGATETO_E7D732_H
#define NOC_W16_DELEGATETO_E7D732_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_delegateto_e7d732_attribution @ tools/delegate_tool_registry.py:get_subagent_attribution */
struct json_t *w16_delegateto_e7d732_attribution(const struct json_t *state);
/* PoP: w16_delegateto_e7d732_resume_spec @ tools/delegate_tool_registry.py:_resume_spec */
struct json_t *w16_delegateto_e7d732_resume_spec(const struct json_t *record);
/* PoP: w16_delegateto_e7d732_public_record @ tools/delegate_tool_registry.py:list_active_subagents */
struct json_t *w16_delegateto_e7d732_public_record(const struct json_t *record);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_DELEGATETO_E7D732_H */
