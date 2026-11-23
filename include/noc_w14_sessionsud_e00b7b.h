#ifndef NOC_W14_SESSIONSUD_E00B7B_H
#define NOC_W14_SESSIONSUD_E00B7B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sessionsud_e00b7b_ask_sudo_request @ gateway/session_sudo_prompt.py:ask_sudo
 *
 * Faithful seam-scoped port of ask_sudo(): the deterministic part is the
 * single "sudo" request it builds for the session's viewers. The side-effecting
 * viewer/transport surface (gateway.session_server_requests.ask_viewer) is a
 * fixture-injected seam. Returns the outgoing request descriptor as a JSON
 * object {"session_id":..., "method":"sudo", "params":{"command":...},
 * "timeout":...}; caller frees. */
struct json_t *w14_sessionsud_e00b7b_ask_sudo_request(const struct json_t *session_id,
                                                     const struct json_t *command,
                                                     const struct json_t *timeout);

/* PoP: w14_sessionsud_e00b7b_sudo_timeout_s @ gateway/session_sudo_prompt.py:SUDO_TIMEOUT_S */
double w14_sessionsud_e00b7b_sudo_timeout_s(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONSUD_E00B7B_H */
