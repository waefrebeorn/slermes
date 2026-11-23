#ifndef NOC_W8_APPROVALACT_58FB2A_H
#define NOC_W8_APPROVALACT_58FB2A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: approvalact_may_answer @ gateway/approval_actor.py:may_answer */
int approvalact_may_answer(struct json_t *authority, const char *actor_id,
                           struct json_t *admin_ids, struct json_t *extra,
                           struct json_t *commands, int admitted);

/* PoP: approvalact_refusal @ gateway/approval_actor.py:_refusal
 *
 * Returns a malloc'd refusal name, or NULL when allowed.  The oracle's
 * gateway.slash_access stub (policy_for_chat_type/SlashAccessPolicy/
 * _coerce_id_list) is mirrored by the fixture's admin_ids/extra fields. */
char *approvalact_refusal(struct json_t *authority, const char *actor_id,
                          struct json_t *admin_ids, struct json_t *extra,
                          struct json_t *commands, int admitted);

/* PoP: approvalact_commands @ gateway/approval_actor.py:approval_commands */
struct json_t *approvalact_commands(const char *choice);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_APPROVALACT_58FB2A_H */
