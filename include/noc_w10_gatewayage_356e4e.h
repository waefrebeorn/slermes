#ifndef NOC_W10_GATEWAYAGE_356E4E_H
#define NOC_W10_GATEWAYAGE_356E4E_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_gatewayage_356e4e_effort_word @ hermes_cli/gateway_agent.py:_effort_word
 * NULL when no override; "none" for enabled=False; the effort string otherwise. */
struct json_t *w10_gatewayage_356e4e_effort_word(const struct json_t *config);
/* PoP: w10_gatewayage_356e4e_review_summary_line @ hermes_cli/gateway_agent.py:review_summary_line
 * "  "+str(text).strip() or NULL for an empty payload/text. */
struct json_t *w10_gatewayage_356e4e_review_summary_line(const struct json_t *payload);
/* PoP: w10_gatewayage_356e4e_clarify_reply @ hermes_cli/gateway_agent.py:clarify_reply
 * reply=NULL models callback=None; otherwise `reply` is the dict the callback returns. */
struct json_t *w10_gatewayage_356e4e_clarify_reply(const struct json_t *reply, const struct json_t *params);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_GATEWAYAGE_356E4E_H */
