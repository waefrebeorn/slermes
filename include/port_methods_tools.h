/*
 * port_methods_tools.h -- pure helpers ported from
 * tui_gateway/methods_tools.py and tui_gateway/user_messages.py.
 *
 * The RPC dispatch / session registry / profile scope stay in Python.
 *
 * All maps are flat NULL-terminated [key, value, ...] arrays.
 */
#ifndef PORT_METHODS_TOOLS_H
#define PORT_METHODS_TOOLS_H

#include <stddef.h>

#define MT_ERR_MAX 1024

/* _str_arg -- str(value or "").strip(). No default: a missing key yields "".
 * Caller frees. */
char *mt_str_arg(const char *const *params, const char *key);

/* busy_message -- the 4009 refusal text; the compressing branch says "wait"
 * instead of "stop the reply". Caller frees. */
char *mt_busy_message(const char *command, int compressing);

/* _err -- the JSON-RPC error envelope. `data` NULL omits the key. */
void mt_err_json(char *buf, size_t cap, const char *rid, int code,
                 const char *msg, const char *data);

/* _mcp_plugin_write_error -- 4090 when a plugin provides the server.
 * Returns 1 when an error was produced, 0 when the write may proceed. */
int mt_plugin_error(const char *rid, const char *name,
                    const char *const *plugins, char *out, size_t cap);

/* _busy_error -- 4009 when the session is running.
 * Returns 1 when an error was produced, 0 when the command may run. */
int mt_busy_error(const char *rid, const char *const *session, const char *cmd,
                  char *out, size_t cap);

#endif /* PORT_METHODS_TOOLS_H */
