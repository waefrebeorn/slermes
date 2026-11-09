/*
 * port_dingtalk_auth.h -- pure helpers ported from hermes_cli/dingtalk_auth.py.
 *
 * The device-flow HTTP calls stay in Python; only the response interpretation
 * is here.
 */
#ifndef PORT_DINGTALK_AUTH_H
#define PORT_DINGTALK_AUTH_H

#include <stddef.h>
#include "hermes_json.h"

/* dingtalk_auth.py:poll_registration's status line - the known states
 * (WAITING/SUCCESS/FAIL/EXPIRED) uppercased, anything else "UNKNOWN".
 * Caller frees. */
char *dt_normalize_poll_status(const char *status);

/* dingtalk_auth.py:poll_registration's field extraction -
 * str(value).strip(), or NULL when absent/blank (`or None`). Caller frees. */
char *dt_str_field(const json_t *data, const char *key);

#endif /* PORT_DINGTALK_AUTH_H */
