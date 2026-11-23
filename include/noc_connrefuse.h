#ifndef NOC_CONNREFUSE_H
#define NOC_CONNREFUSE_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: connrefuse_constant @ tui_gateway/connector_refusals.py:INVALID_PARAMS
 * Returns [code, reason, message] for a named refusal constant, or JSON null. */
struct json_t *connrefuse_constant(const char *name);
/* PoP: connrefuse_reissue_refusal @ tui_gateway/connector_refusals.py:REISSUE_REFUSALS */
struct json_t *connrefuse_reissue_refusal(const char *key);
/* PoP: connrefuse_auth_refusal @ tui_gateway/connector_refusals.py:auth_refusal */
struct json_t *connrefuse_auth_refusal(const char *code, int status);
#ifdef __cplusplus
}
#endif
#endif /* NOC_CONNREFUSE_H */
