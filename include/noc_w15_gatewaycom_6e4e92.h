#ifndef NOC_W15_GATEWAYCOM_6E4E92_H
#define NOC_W15_GATEWAYCOM_6E4E92_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_gatewaycom_6e4e92_deferred_pick_text @ acp_adapter/gateway_commands.py:deferred_pick_text */
struct json_t *w15_gatewaycom_6e4e92_deferred_pick_text(const struct json_t *result);

/* PoP: w15_gatewaycom_6e4e92_refused_pick_text @ acp_adapter/gateway_commands.py:refused_pick_text */
struct json_t *w15_gatewaycom_6e4e92_refused_pick_text(const struct json_t *update);

/* PoP: w15_gatewaycom_6e4e92_summary_text @ acp_adapter/gateway_commands.py:_summary_text */
struct json_t *w15_gatewaycom_6e4e92_summary_text(const struct json_t *result);

/* PoP: w15_gatewaycom_6e4e92_model_text @ acp_adapter/gateway_commands.py:_model_text */
struct json_t *w15_gatewaycom_6e4e92_model_text(const char *label, const struct json_t *fields);

/* PoP: w15_gatewaycom_6e4e92_tools_text @ acp_adapter/gateway_commands.py:_tools_text */
struct json_t *w15_gatewaycom_6e4e92_tools_text(const struct json_t *info);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_GATEWAYCOM_6E4E92_H */
