/*
 * port_methods_tools.c
 *
 * Pure helpers ported from tui_gateway/methods_tools.py (112 defs, previously 0
 * ported) and tui_gateway/user_messages.py.
 *
 *   mt_str_arg        <- _str_arg
 *   mt_busy_message   <- user_messages.py:busy_message
 *   mt_plugin_error   <- _mcp_plugin_write_error
 *   mt_busy_error     <- _busy_error
 *   mt_err_json       <- server.py:_err
 *
 * The RPC dispatch / session registry / profile scope stay in Python.
 */

#include "port_methods_tools.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _str_arg  (methods_tools.py:_str_arg)
 *
 *      return str(params.get(key) or "").strip()
 *
 *  Note the difference from methods_session.py:_str_param: there is NO default
 *  here, so a missing or empty key yields "" rather than a caller-supplied
 *  default. `params` is a flat [key, value, ...] array; caller frees.
 * =========================================================================== */

/* PoP: _str_arg @ tui_gateway/methods_tools.py:_str_arg */
char *mt_str_arg(const char *const *params, const char *key)
{
    const char *val = NULL;
    for (int i = 0; params && params[i]; i += 2)
        if (strcmp(params[i], key) == 0) { val = params[i + 1]; break; }

    char *s;
    if (val && val[0]) s = strdup(val);
    else { s = (char *)malloc(1); if (s) s[0] = '\0'; }
    if (!s) return NULL;

    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) s[--b] = '\0';
    memmove(s, s + a, b - a + 1);
    return s;
}

/* ===========================================================================
 *  busy_message  (user_messages.py:busy_message)
 *
 *      if compressing:
 *          return f"session busy — a /compress is running; run /{command.lstrip('/')} when it finishes."
 *      return (f"session busy — Hermes is still replying. Stop the current reply first (Stop button, "
 *              f"or Ctrl+C in a terminal), then run /{command.lstrip('/')}.")
 *
 *  The two branches are deliberately different: a manual /compress holds the
 *  session busy but nothing is replying, so "Stop the current reply" would be
 *  wrong advice — say to wait instead. Caller frees.
 * =========================================================================== */

/* PoP: busy_message @ tui_gateway/user_messages.py:busy_message */
char *mt_busy_message(const char *command, int compressing)
{
    /* command.lstrip('/') - strip LEADING slashes only */
    const char *cmd = command ? command : "";
    while (*cmd == '/') cmd++;

    size_t n = strlen(cmd) + 200;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    if (compressing)
        snprintf(out, n,
                 "session busy \xE2\x80\x94 a /compress is running; run /%s when it finishes.", cmd);
    else
        snprintf(out, n,
                 "session busy \xE2\x80\x94 Hermes is still replying. Stop the current reply first "
                 "(Stop button, or Ctrl+C in a terminal), then run /%s.", cmd);
    return out;
}

/* ===========================================================================
 *  _err  (server.py:_err)
 *
 *      error = {"code": code, "message": msg, **({"data": data} if data is not None else {})}
 *      return {"jsonrpc": "2.0", "id": rid, "error": error}
 *
 *  Emits the JSON-RPC envelope. `data` is optional (NULL omits it). `buf` must
 *  be at least MT_ERR_MAX bytes.
 * =========================================================================== */

/* PoP: _err @ tui_gateway/server.py:_err */
void mt_err_json(char *buf, size_t cap, const char *rid, int code,
                 const char *msg, const char *data)
{
    if (!buf || cap == 0) return;

    /* JSON-escape the message and the optional data into scratch buffers */
    size_t mlen = msg ? strlen(msg) : 0;
    size_t dlen = data ? strlen(data) : 0;
    char *em = (char *)malloc(mlen * 6 + 1);
    char *ed = (char *)malloc(dlen * 6 + 1);
    if (!em || !ed) { free(em); free(ed); buf[0] = '\0'; return; }

    size_t k = 0;
    for (size_t i = 0; i < mlen; i++) {
        unsigned char c = (unsigned char)msg[i];
        if (c == '"' || c == '\\') { em[k++] = '\\'; em[k++] = (char)c; }
        else if (c < 0x20) { k += (size_t)snprintf(em + k, 7, "\\u%04x", c); }
        else em[k++] = (char)c;
    }
    em[k] = '\0';
    k = 0;
    for (size_t i = 0; i < dlen; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '"' || c == '\\') { ed[k++] = '\\'; ed[k++] = (char)c; }
        else if (c < 0x20) { k += (size_t)snprintf(ed + k, 7, "\\u%04x", c); }
        else ed[k++] = (char)c;
    }
    ed[k] = '\0';

    /* rid is a JSON-RPC request id: a STRING when present, bare JSON null when
     * not. The two cannot share a format string, so the prefix is built first. */
    char idbuf[128];
    if (rid) snprintf(idbuf, sizeof(idbuf), "\"%s\"", rid);
    else snprintf(idbuf, sizeof(idbuf), "null");

    if (data)
        snprintf(buf, cap,
                 "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\",\"data\":\"%s\"}}",
                 idbuf, code, em, ed);
    else
        snprintf(buf, cap,
                 "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 idbuf, code, em);
    free(em);
    free(ed);
}

/* ===========================================================================
 *  _mcp_plugin_write_error  (methods_tools.py:_mcp_plugin_write_error)
 *
 *      plugin = plugins.get(name)
 *      if plugin is not None:
 *          return _err(rid, 4090, f"server '{name}' is provided by plugin '{plugin}' and cannot be modified")
 *      return None
 *
 *  4090 = a plugin-provided server is read-only. Returns 1 when an error was
 *  produced (the message is in `out`), 0 when the write may proceed.
 *  `plugins` is a flat [name, provider, ...] array.
 * =========================================================================== */

/* PoP: _mcp_plugin_write_error @ tui_gateway/methods_tools.py:_mcp_plugin_write_error */
int mt_plugin_error(const char *rid, const char *name,
                    const char *const *plugins, char *out, size_t cap)
{
    const char *provider = NULL;
    for (int i = 0; plugins && plugins[i]; i += 2)
        if (strcmp(plugins[i], name) == 0) { provider = plugins[i + 1]; break; }
    if (!provider) return 0;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "server '%s' is provided by plugin '%s' and cannot be modified",
             name ? name : "", provider);
    mt_err_json(out, cap, rid, 4090, msg, NULL);
    return 1;
}

/* ===========================================================================
 *  _busy_error  (methods_tools.py:_busy_error)
 *
 *      if session.get("running"):
 *          return _err(rid, 4009, busy_message(cmd, bool(session.get("_manual_compress_active"))))
 *      return None
 *
 *  4009 = a history-mutating command arrived while the session is busy.
 *  `session` is a flat [key, value, ...] array. Returns 1 when an error was
 *  produced, 0 when the command may run.
 * =========================================================================== */

/* PoP: _busy_error @ tui_gateway/methods_tools.py:_busy_error */
int mt_busy_error(const char *rid, const char *const *session, const char *cmd,
                  char *out, size_t cap)
{
    const char *running = NULL, *compressing = NULL;
    for (int i = 0; session && session[i]; i += 2) {
        if (strcmp(session[i], "running") == 0) running = session[i + 1];
        else if (strcmp(session[i], "_manual_compress_active") == 0) compressing = session[i + 1];
    }
    if (!(running && strcmp(running, "1") == 0)) return 0;

    int comp = (compressing && strcmp(compressing, "1") == 0);
    char *msg = mt_busy_message(cmd, comp);
    if (!msg) { out[0] = '\0'; return 1; }
    mt_err_json(out, cap, rid, 4009, msg, NULL);
    free(msg);
    return 1;
}
