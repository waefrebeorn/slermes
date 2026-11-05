/*
 * port_aux_outcome_event.h — pure helpers, two source modules:
 *   agent/auxiliary_call_outcome.py  embedded_provider_error, is_cancellation
 *   gateway/platforms/event.py       clean_attachment_filename, _command_text
 */
#ifndef PORT_AUX_OUTCOME_EVENT_H
#define PORT_AUX_OUTCOME_EVENT_H

/* event.py clean_attachment_filename: untrusted filename -> display-safe
 * basename (drop directory parts + Cc/Cf control/format chars, reject
 * ""/"."/"..", cap to 255 keeping the extension), or "" when unusable.
 * Returns malloc'd, caller frees. */
char *eve_clean_attachment_filename(const char *name);

/* event.py MessageEvent._command_text: text with leading @image:/@file:/@url:
 * refs stripped (each "@tag:<non-newline>\n?" one-or-more, case-insensitive),
 * then leading whitespace removed. Returns malloc'd, caller frees. */
char *eve_command_text(const char *text);

/* auxiliary_call_outcome.py embedded_provider_error: the "error" member of a
 * 200-body dict (a non-empty object -> it, else NULL). Returns malloc'd JSON.
 */
char *aco_embedded_provider_error(const char *response_json);

/* auxiliary_call_outcome.py is_cancellation(error): True when the error is not
 * an Exception, or is an InterruptedError. is_exception/is_interrupted are the
 * parameterized facts. */
int aco_is_cancellation(int is_exception, int is_interrupted);

#endif /* PORT_AUX_OUTCOME_EVENT_H */
