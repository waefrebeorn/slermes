#ifndef SLERMES_PORT_BROWSER_SUPERVISOR_H
#define SLERMES_PORT_BROWSER_SUPERVISOR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_browser_supervisor_state port_browser_supervisor_state_t;

/* Lifecycle */
port_browser_supervisor_state_t *port_browser_supervisor_init(void);
void port_browser_supervisor_cleanup(port_browser_supervisor_state_t *state);

/* CDP redaction */
char *port_browser_supervisor_redact_cdp_error_text(port_browser_supervisor_state_t *state, const char *error_text);
char *port_browser_supervisor_redact_supervisor_text(port_browser_supervisor_state_t *state, const char *value);

/* Dialog handling */
json_t *port_browser_supervisor_respond_to_dialog(port_browser_supervisor_state_t *state, const char *session_key, bool accept, const char *prompt_text);
json_t *port_browser_supervisor_evaluate_runtime(port_browser_supervisor_state_t *state, const char *session_key, const char *expression, bool return_by_value, bool await_promise);

/* Thread lifecycle */
void *port_browser_supervisor_thread_main(port_browser_supervisor_state_t *state, void *arg);

/* CDP event handlers */
void port_browser_supervisor_archive_dialog_locked(port_browser_supervisor_state_t *state);
void port_browser_supervisor_on_frame_attached(port_browser_supervisor_state_t *state, const char *frame_id);
void port_browser_supervisor_on_frame_navigated(port_browser_supervisor_state_t *state, const char *frame_id);
void port_browser_supervisor_on_frame_detached(port_browser_supervisor_state_t *state, const char *frame_id);
void port_browser_supervisor_on_target_detached(port_browser_supervisor_state_t *state, const char *target_id);
void port_browser_supervisor_on_console(port_browser_supervisor_state_t *state, const char *message);
json_t *port_browser_supervisor_build_frame_tree_locked(port_browser_supervisor_state_t *state);

/* Supervisor lifecycle */
json_t *port_browser_supervisor_get_or_start(port_browser_supervisor_state_t *state, const char *session_key);
void port_browser_supervisor_stop_all(port_browser_supervisor_state_t *state);
json_t *port_browser_supervisor_attach_initial_page(port_browser_supervisor_state_t *state);
void port_browser_supervisor_install_dialog_bridge(port_browser_supervisor_state_t *state, const char *session_id);
void port_browser_supervisor_on_event(port_browser_supervisor_state_t *state, const char *method, const char *params_json, const char *session_id);

/* Supervisor event handlers */
void port_browser_supervisor_on_dialog_opening(port_browser_supervisor_state_t *state, const char *dialog_type, const char *message, const char *default_prompt, const char *session_id, const char *frame_id);
void port_browser_supervisor_auto_handle_dialog(port_browser_supervisor_state_t *state, const char *dialog_id, bool accept, const char *prompt_text);
void port_browser_supervisor_on_dialog_closed(port_browser_supervisor_state_t *state, const char *session_id);
void port_browser_supervisor_on_fetch_paused(port_browser_supervisor_state_t *state, const char *request_id, const char *url, const char *session_id, const char *frame_id);
void port_browser_supervisor_on_frame_attached_with_session(port_browser_supervisor_state_t *state, const char *frame_id, const char *session_id);
void port_browser_supervisor_on_frame_navigated_with_url(port_browser_supervisor_state_t *state, const char *frame_id, const char *url, const char *session_id);
void port_browser_supervisor_on_target_attached(port_browser_supervisor_state_t *state, const char *target_id, const char *session_id);
void port_browser_supervisor_on_console_with_level(port_browser_supervisor_state_t *state, const char *text, const char *level);

/* Bridge / CDP commands */
json_t *port_browser_supervisor_fulfill_bridge_request(port_browser_supervisor_state_t *state, const char *request_id, bool accept, const char *prompt_text);
json_t *port_browser_supervisor_handle_dialog_cdp(port_browser_supervisor_state_t *state, const char *session_key, const char *dialog_id, bool accept, const char *prompt_text);
void port_browser_supervisor_dialog_timeout_expired(port_browser_supervisor_state_t *state, const char *dialog_id);
void port_browser_supervisor_enable_child_domains(port_browser_supervisor_state_t *state, const char *session_id);
char *port_browser_supervisor_cdp(port_browser_supervisor_state_t *state, const char *method, json_t *params);

#endif /* SLERMES_PORT_BROWSER_SUPERVISOR_H */