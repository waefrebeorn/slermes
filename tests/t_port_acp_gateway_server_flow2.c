/* t_port_acp_gateway_server_flow2.c — fixture-driven FLOW oracle harness for the
 * C11 port of the 31-func faithful subset of acp_adapter/gateway_server.py
 * (src/acp_adapter/acp_gateway_server_flow2.c).
 *
 * Reads JSONL cases from stdin: {"func":<live_name>,"in":{"argv":[self_state,..],
 * "kw":{..},"seams":{name: const|[queue]}}}. Emits one JSON line per case:
 *   {"func":..,"in":..,"ret":<value|raise spec>,"trace":[{"ev","args","ret"}],"st":{}}
 * The port file (included by bare name) implements every func as static agsf2_*.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "acp_adapter/acp_gateway_server_flow2.c"

typedef json_t *(*agsf2_fn)(agsf2_io_t *, const json_t *, const json_t *);
typedef struct { const char *name; agsf2_fn fn; } agsf2_dispatch_t;

static const agsf2_dispatch_t AGSF2_DISPATCH[] = {
    { "__init__", agsf2___init__ },
    { "on_connect", agsf2_on_connect },
    { "_observe_client_message", agsf2__observe_client_message },
    { "initialize", agsf2_initialize },
    { "authenticate", agsf2_authenticate },
    { "new_session", agsf2_new_session },
    { "_client", agsf2__client },
    { "_adopt", agsf2__adopt },
    { "_move_workspace", agsf2__move_workspace },
    { "_rebind_editor_mcp", agsf2__rebind_editor_mcp },
    { "load_session", agsf2_load_session },
    { "resume_session", agsf2_resume_session },
    { "cancel", agsf2_cancel },
    { "fork_session", agsf2_fork_session },
    { "set_session_model", agsf2_set_session_model },
    { "_confirm_deferred_pick", agsf2__confirm_deferred_pick },
    { "_model_confirmer", agsf2__model_confirmer },
    { "set_session_mode", agsf2_set_session_mode },
    { "set_config_option", agsf2_set_config_option },
    { "_idle_control", agsf2__idle_control },
    { "list_sessions", agsf2_list_sessions },
    { "prompt", agsf2_prompt },
    { "_prepare_submit", agsf2__prepare_submit },
    { "_refresh_unknown", agsf2__refresh_unknown },
    { "_events", agsf2__events },
    { "_project", agsf2__project },
    { "_gap_guard", agsf2__gap_guard },
    { "_answer_clarify", agsf2__answer_clarify },
    { "_session_fields", agsf2__session_fields },
    { "_teardown", agsf2__teardown },
    { "_notify_unknown", agsf2__notify_unknown },
    { NULL, NULL }
};

static char *agsf2h_readline(FILE *f)
{
    size_t cap = 8192, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c = EOF;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (n == 0 && c == EOF) { free(buf); return NULL; }
    buf[n] = 0;
    while (n > 0 && buf[n - 1] == '\r') buf[--n] = 0;
    return buf;
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; } }
    char *line;
    while ((line = agsf2h_readline(f)) != NULL) {
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        free(line);
        if (!rec || !json_is_object(rec)) { if (rec) json_free(rec); free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *argv_node = in ? json_obj_get(in, "argv") : NULL;
        json_t *kw_node = in ? json_obj_get(in, "kw") : NULL;
        json_t *seams_node = in ? json_obj_get(in, "seams") : NULL;
        if (!json_is_array(argv_node)) argv_node = json_array();
        if (!json_is_object(kw_node)) kw_node = json_object();
        if (!json_is_object(seams_node)) seams_node = json_object();

        agsf2_fn fn = NULL;
        for (int i = 0; AGSF2_DISPATCH[i].name; i++)
            if (strcmp(func, AGSF2_DISPATCH[i].name) == 0) { fn = AGSF2_DISPATCH[i].fn; break; }

        agsf2_io_t io;
        io.seams = seams_node;
        io.cursors = json_object();
        io.trace = json_array();
        json_t *ret;
        if (fn) ret = fn(&io, argv_node, kw_node);
        else ret = agsf2_mkraise("RuntimeError", "unknown func");

        json_t *out = json_object();
        json_set(out, "func", json_string(func));
        /* Echo the case input verbatim: the oracle re-drives the LIVE function
         * with these argv/kw/seams. Without this field every case is driven
         * with empty args, so the diff compares real-arg C output against a
         * zero-arg Python call (261/266 false red). Documented output shape. */
        json_set(out, "in", in ? json_copy(in) : json_object());
        json_set(out, "trace", io.trace);
        json_set(out, "ret", ret ? json_copy(ret) : json_null());
        json_set(out, "st", json_object());
        char *serialized = json_serialize(out);
        fputs(serialized ? serialized : "{}\n", stdout);
        fputc('\n', stdout);
        free(serialized);
        json_free(out);
        json_free(ret);
        json_free(io.cursors);
        json_free(rec);
        free(err);
    }
    if (f != stdin) fclose(f);
    return 0;
}
