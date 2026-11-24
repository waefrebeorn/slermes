/*
 * cli.c - C11 port of upstream plugins/google_meet/node/cli.py
 * (gate w24_cli_c07d7e).
 *
 * PoP: w24_cli_c07d7e_dispatch @ plugins/google_meet/node/cli.py:node_command
 * PoP: w24_cli_c07d7e_format_list @ plugins/google_meet/node/cli.py:_cmd_list
 * PoP: w24_cli_c07d7e_approve_message @ plugins/google_meet/node/cli.py:_cmd_approve
 * PoP: w24_cli_c07d7e_remove_result @ plugins/google_meet/node/cli.py:_cmd_remove
 * PoP: w24_cli_c07d7e_run_banner @ plugins/google_meet/node/cli.py:_cmd_run
 * PoP: w24_cli_c07d7e_ping_fail @ plugins/google_meet/node/cli.py:_cmd_ping
 * PoP: w24_cli_c07d7e_ping_missing @ plugins/google_meet/node/cli.py:_cmd_ping
 * PoP: w24_cli_c07d7e_constants @ plugins/google_meet/node/cli.py:register_cli
 *
 * Every handler in this module touches a seam -- the registry file (NodeRegistry),
 * the node server (ensure_token/serve) and the network (NodeClient.ping).
 * None of them is entered here: the registry answers (the node list, whether the
 * removal succeeded, whether a name exists) and the ping outcome arrive as
 * values, and what is ported is what the CLI prints and returns around them,
 * plus the `_COMMANDS` dispatch table and the argparse defaults.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "noc_w24_cli_c07d7e.h"

/* register_cli() defaults */
static const char *const CLI_DEFAULT_HOST = "0.0.0.0";
static const double CLI_DEFAULT_PORT = 18789;
static const char *const CLI_DEFAULT_DISPLAY_NAME = "hermes-meet-node";

/* _COMMANDS */
static const char *const CLI_COMMANDS[6] = {"run", "list", "approve", "remove", "status", "ping"};
static const char *const CLI_HANDLERS[6] = {
    "_cmd_run", "_cmd_list", "_cmd_approve", "_cmd_remove", "_cmd_ping", "_cmd_ping",
};

/* --------------------------------------------------------- tiny builder */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} cli_buf;

static void cli_buf_addn(cli_buf *b, const char *s, size_t n) {
    if (!b || !s) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void cli_buf_add(cli_buf *b, const char *s) {
    if (s) cli_buf_addn(b, s, strlen(s));
}

static void cli_buf_addf(cli_buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) cli_buf_addn(b, tmp, (size_t)n);
}

static json_t *cli_buf_json(cli_buf *b) {
    json_t *out;
    if (!b->p) return json_string("");
    out = json_string(b->p);
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

/* ------------------------------------------------------------ py values */

static const json_t *cli_arg(const json_t *req, const char *key) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
}

static const char *cli_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int cli_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* repr(str): quote choice and escapes follow CPython. */
static void cli_buf_add_repr(cli_buf *b, const char *s, int quoted) {
    int has_single = 0, has_double = 0;
    char q = '\'';
    const char *p;

    if (!quoted) { cli_buf_add(b, s); return; }
    for (p = s; *p; p++) {
        if (*p == '\'') has_single = 1;
        if (*p == '"') has_double = 1;
    }
    if (has_single && !has_double) q = '"';
    cli_buf_addn(b, &q, 1);
    for (p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)q || c == '\\') {
            char esc[2];
            esc[0] = '\\';
            esc[1] = (char)c;
            cli_buf_addn(b, esc, 2);
        } else if (c == '\n') {
            cli_buf_add(b, "\\n");
        } else if (c == '\r') {
            cli_buf_add(b, "\\r");
        } else if (c == '\t') {
            cli_buf_add(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            cli_buf_addf(b, "\\x%02x", c);
        } else {
            cli_buf_addn(b, (const char *)&c, 1);
        }
    }
    cli_buf_addn(b, &q, 1);
}

/* JSON-escaped string contents, as json.dumps(ensure_ascii=True) writes them. */
static void cli_buf_add_json_str(cli_buf *b, const char *s) {
    const unsigned char *p;
    cli_buf_addn(b, "\"", 1);
    for (p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"') cli_buf_add(b, "\\\"");
        else if (c == '\\') cli_buf_add(b, "\\\\");
        else if (c == '\n') cli_buf_add(b, "\\n");
        else if (c == '\r') cli_buf_add(b, "\\r");
        else if (c == '\t') cli_buf_add(b, "\\t");
        else if (c < 0x20) cli_buf_addf(b, "\\u%04x", c);
        else cli_buf_addn(b, (const char *)&c, 1);
    }
    cli_buf_addn(b, "\"", 1);
}

/* --------------------------------------------------------------- functions */

json_t *w24_cli_c07d7e_dispatch(const json_t *req) {
    const json_t *cmd = cli_arg(req, "cmd");
    const char *name = cli_str(cmd);
    const char *handler = NULL;
    json_t *out = json_object();
    size_t i;

    if (name) {
        for (i = 0; i < 6; i++)
            if (strcmp(name, CLI_COMMANDS[i]) == 0) handler = CLI_HANDLERS[i];
    }
    json_set(out, "handler", handler ? json_string(handler) : json_null());
    if (handler) {
        json_set(out, "rc", json_null());
        json_set(out, "error", json_null());
    } else {
        cli_buf b;
        memset(&b, 0, sizeof(b));
        cli_buf_add(&b, "unknown node command: ");
        if (cmd && cmd->type == JSON_STRING) {
            cli_buf_add_repr(&b, cmd->str_val ? cmd->str_val : "", 1);
        } else if (cmd && cmd->type == JSON_NUMBER) {
            cli_buf_addf(&b, "%.0f", cmd->num_val);
        } else if (cmd && cmd->type == JSON_BOOL) {
            cli_buf_add(&b, cmd->bool_val ? "True" : "False");
        } else {
            cli_buf_add(&b, "None");
        }
        json_set(out, "rc", json_number(2));
        json_set(out, "error", cli_buf_json(&b));
    }
    return out;
}

json_t *w24_cli_c07d7e_format_list(const json_t *req) {
    const json_t *nodes = cli_arg(req, "nodes");
    cli_buf b;
    size_t i, n;

    memset(&b, 0, sizeof(b));
    n = (nodes && nodes->type == JSON_ARRAY) ? json_len(nodes) : 0;
    if (n == 0) {
        cli_buf_add(&b, "no nodes registered\n");
        return cli_buf_json(&b);
    }
    for (i = 0; i < n; i++) {
        const json_t *node = json_get(nodes, i);
        const char *name = node && node->type == JSON_OBJECT
                               ? cli_str(json_obj_get(node, "name")) : NULL;
        const char *url = node && node->type == JSON_OBJECT
                              ? cli_str(json_obj_get(node, "url")) : NULL;
        const char *token = node && node->type == JSON_OBJECT
                                ? cli_str(json_obj_get(node, "token")) : NULL;
        cli_buf_add(&b, name ? name : "");
        cli_buf_addn(&b, "\t", 1);
        cli_buf_add(&b, url ? url : "");
        cli_buf_add(&b, "\ttoken=");
        if (token) {
            size_t k = 0;
            while (token[k] && k < 6) k++;
            cli_buf_addn(&b, token, k);
        }
        cli_buf_add(&b, "\xe2\x80\xa6\n");   /* … */
    }
    return cli_buf_json(&b);
}

json_t *w24_cli_c07d7e_approve_message(const json_t *req) {
    const char *name = cli_str(cli_arg(req, "name"));
    const char *url = cli_str(cli_arg(req, "url"));
    cli_buf b;

    memset(&b, 0, sizeof(b));
    cli_buf_add(&b, "approved node ");
    cli_buf_add_repr(&b, name ? name : "", 1);
    cli_buf_add(&b, " at ");
    cli_buf_add(&b, url ? url : "");
    cli_buf_addn(&b, "\n", 1);
    return cli_buf_json(&b);
}

json_t *w24_cli_c07d7e_remove_result(const json_t *req) {
    const char *name = cli_str(cli_arg(req, "name"));
    int ok = cli_truthy(cli_arg(req, "ok"));
    cli_buf b;
    json_t *out = json_object();

    memset(&b, 0, sizeof(b));
    if (ok) cli_buf_add(&b, "removed ");
    else cli_buf_add(&b, "no such node: ");
    cli_buf_add_repr(&b, name ? name : "", 1);
    cli_buf_addn(&b, "\n", 1);
    json_set(out, "message", cli_buf_json(&b));
    json_set(out, "rc", json_number(ok ? 0.0 : 1.0));
    return out;
}

json_t *w24_cli_c07d7e_run_banner(const json_t *req) {
    const char *host = cli_str(cli_arg(req, "host"));
    const char *name = cli_str(cli_arg(req, "display_name"));
    const char *token = cli_str(cli_arg(req, "token"));
    const json_t *port = cli_arg(req, "port");
    cli_buf b;

    memset(&b, 0, sizeof(b));
    cli_buf_add(&b, "[meet-node] display_name=");
    cli_buf_add(&b, name ? name : "");
    cli_buf_add(&b, "\n[meet-node] listening on ws://");
    cli_buf_add(&b, host ? host : "");
    cli_buf_addn(&b, ":", 1);
    if (port && port->type == JSON_NUMBER) cli_buf_addf(&b, "%.0f", port->num_val);
    cli_buf_add(&b, "\n[meet-node] token (copy to gateway): ");
    cli_buf_add(&b, token ? token : "");
    cli_buf_add(&b, "\n[meet-node] approve with:\n             hermes meet node approve <name> ws://<host>:");
    if (port && port->type == JSON_NUMBER) cli_buf_addf(&b, "%.0f", port->num_val);
    cli_buf_addn(&b, " ", 1);
    cli_buf_add(&b, token ? token : "");
    cli_buf_addn(&b, "\n", 1);
    return cli_buf_json(&b);
}

json_t *w24_cli_c07d7e_ping_fail(const json_t *req) {
    const char *name = cli_str(cli_arg(req, "name"));
    const char *error = cli_str(cli_arg(req, "error"));
    cli_buf b;
    json_t *out = json_object();
    (void)name;

    memset(&b, 0, sizeof(b));
    cli_buf_add(&b, "{\"ok\": false, \"error\": ");
    cli_buf_add_json_str(&b, error ? error : "");
    cli_buf_add(&b, "}\n");
    json_set(out, "stdout", cli_buf_json(&b));
    json_set(out, "rc", json_number(1));
    return out;
}

json_t *w24_cli_c07d7e_ping_missing(const json_t *req) {
    const char *name = cli_str(cli_arg(req, "name"));
    cli_buf b;
    json_t *out = json_object();

    memset(&b, 0, sizeof(b));
    cli_buf_add(&b, "no such node: ");
    cli_buf_add_repr(&b, name ? name : "", 1);
    cli_buf_addn(&b, "\n", 1);
    json_set(out, "stderr", cli_buf_json(&b));
    json_set(out, "rc", json_number(1));
    return out;
}

json_t *w24_cli_c07d7e_constants(void) {
    json_t *o = json_object();
    json_t *commands = json_array();
    size_t i;
    json_set(o, "default_host", json_string(CLI_DEFAULT_HOST));
    json_set(o, "default_port", json_number(CLI_DEFAULT_PORT));
    json_set(o, "default_display_name", json_string(CLI_DEFAULT_DISPLAY_NAME));
    for (i = 0; i < 6; i++) json_append(commands, json_string(CLI_COMMANDS[i]));
    json_set(o, "commands", commands);
    return o;
}
