#ifndef NOC_W14_SESSIONCON_1A39CB_H
#define NOC_W14_SESSIONCON_1A39CB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessioncon_1a39cb_session_owned @ gateway/session_connections.py:session_owned
 * True when params.owner is an object whose type == "session". */
struct json_t *w14_sessioncon_1a39cb_session_owned(const struct json_t *params);
/* PoP: w14_sessioncon_1a39cb_methods @ gateway/session_connections.py:METHODS
 * The four connection methods this authority handles, in order. */
struct json_t *w14_sessioncon_1a39cb_methods(void);
/* PoP: w14_sessioncon_1a39cb_connector_refusal @ gateway/session_connections.py:ConnectorRefusal.__init__
 * Wire-shape refusal: wire_code, wire_message, reason(str), detail, field, args. */
struct json_t *w14_sessioncon_1a39cb_connector_refusal(const struct json_t *code, const struct json_t *reason,
                                      const struct json_t *message);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONCON_1A39CB_H */
