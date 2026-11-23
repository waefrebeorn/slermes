#ifndef NOC_W9_ANONSIGNIN_37DFD1_H
#define NOC_W9_ANONSIGNIN_37DFD1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_anonsignin_37dfd1_format_wait_line @ hermes_cli/anon_sign_in.py:format_wait_line
 * "Waiting for sign-in, up to {minutes}." with ceil(max(0,int(x))/60), min 1. */
struct json_t *w9_anonsignin_37dfd1_format_wait_line(const struct json_t *expires_in);

/* PoP: w9_anonsignin_37dfd1_completed_copy @ hermes_cli/anon_sign_in.py:Completed.copy / copy_terminal
 * Faithful Completed._lines(no_default) using the chat or terminal no-default copy. */
struct json_t *w9_anonsignin_37dfd1_completed_copy(const struct json_t *email,
                                                  const struct json_t *model,
                                                  int model_changed,
                                                  const struct json_t *handoff,
                                                  int terminal);

/* PoP: w9_anonsignin_37dfd1_code_copy_with_wait @ hermes_cli/anon_sign_in.py:Code.copy_with_wait */
struct json_t *w9_anonsignin_37dfd1_code_copy_with_wait(const struct json_t *expires_in);

/* PoP: w9_anonsignin_37dfd1_state_copy @ hermes_cli/anon_sign_in.py:<state>.copy
 * Constant in-chat copy for SignInState / Code / Waiting / Cancelled / TimedOut /
 * AlreadySignedIn. */
struct json_t *w9_anonsignin_37dfd1_state_copy(const char *kind, const struct json_t *detail);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_ANONSIGNIN_37DFD1_H */
