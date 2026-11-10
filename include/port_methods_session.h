/*
 * port_methods_session.h -- pure helpers ported from
 * tui_gateway/methods_session.py.
 *
 * The RPC dispatch / db / session-registry half stays in Python.
 *
 * All maps are flat NULL-terminated [key, value, ...] arrays.
 */
#ifndef PORT_METHODS_SESSION_H
#define PORT_METHODS_SESSION_H

#include <stddef.h>

/* utils.py:is_truthy_value -- `kind` carries the Python type:
 * 0 = None, 2 = str, 3 = int/bool (use ival's truthiness). */
int ms_is_truthy(int kind, const char *sval, int ival, int dflt);

/* _str_param -- str(value or "").strip() or default. Caller frees. */
char *ms_str_param(const char *const *params, const char *key, const char *dflt);

/* _flag -- is_truthy_value(params.get(name, False)); a missing key is off. */
int ms_flag(const char *const *params, const char *name);

/* _int_param -- int(value) with default for missing / unparsable. */
int ms_int_param(const char *const *params, const char *key, int dflt);

/* _denied_source -- the source is in {kanban, tool, oneshot}. */
int ms_denied_source(const char *const *row);

/* _auto_resume_denied_source -- _denied_source OR source == "unknown". */
int ms_auto_resume_denied(const char *const *row);

#endif /* PORT_METHODS_SESSION_H */
