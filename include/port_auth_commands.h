/*
 * port_auth_commands.h — pure helpers from hermes_cli/auth_commands.py.
 * The entry-object fields the live functions read are parameters here.
 */
#ifndef PORT_AUTH_COMMANDS_H
#define PORT_AUTH_COMMANDS_H

/* _display_source(source): text after "manual:" when source starts with that,
 * else the source unchanged. Returns malloc'd, caller frees. */
char *acm_display_source(const char *source);

/* _classify_exhausted_status: classify a credential-pool entry's last error into
 * (label, show_retry_window). code/reason/message are the entry's
 * last_error_code / last_error_reason / last_error_message. Returns malloc'd
 * "label,1" or "label,0" (label = rate-limited|auth failed|exhausted). */
char *acm_classify_exhausted(const char *code_or_null,
                             const char *reason, const char *message);

/* _format_exhausted_status's wait-string core: given `remaining` seconds (>=0),
 * render "Xd Yh"/"Xh Ym"/"Xm Ys"/"Ns" (first non-zero unit + next, two max).
 * Returns malloc'd, caller frees. */
char *acm_format_wait_left(long long remaining);

#endif /* PORT_AUTH_COMMANDS_H */
