/* t_port_acp_gateway_server_flow.c — fixture-driven FLOW oracle harness for the
 * C11 port of acp_adapter/gateway_server.py (src/acp_adapter/acp_gateway_server_flow.c).
 *
 * Reads JSONL cases from stdin: {"func":<live_name>,"in":{"argv":[..],"kw":{..},
 * "seams":{name: const | [queue]}}}. For methods argv[0] is the self-state object.
 * Emits one JSON line per case:
 *   {"func":..,"ret":<value|raise spec>,"trace":[{"ev","args","ret"}],"st":{}}
 * The port file (included by bare name) implements every func as static agsf_*.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "acp_adapter/acp_gateway_server_flow.c"

typedef json_t *(*agsf_fn)(agsf_io_t *, const json_t *, const json_t *);
typedef struct { const char *name; agsf_fn fn; } agsf_dispatch_t;

static const agsf_dispatch_t AGSF_DISPATCH[] = {
    { "_slash_command", agsf__slash_command },
    { "_tools_changed_notice", agsf__tools_changed_notice },
    { "_bypass_route", agsf__bypass_route },
    { "_editor_id", agsf__editor_id },
    { "_moved", agsf__moved },
    { "_transport_closed", agsf__transport_closed },
    { "_held_session", agsf__held_session },
    { "_has_unknown", agsf__has_unknown },
    { "_pending_event_is_new", agsf__pending_event_is_new },
    { "_blocked_admission", agsf__blocked_admission },
    { "_unknown_recovery", agsf__unknown_recovery },
    { "_request_live_reasoning", agsf__request_live_reasoning },
    { "_after_attach", agsf__after_attach },
    { "_admission_from_receipt", agsf__admission_from_receipt },
    { "_after_ack", agsf__after_ack },
    { "_project_title", agsf__project_title },
    { "_project_usage", agsf__project_usage },
    { "_permission", agsf__permission },
    { "_project_config", agsf__project_config },
    { "_answer_permission", agsf__answer_permission },
    { NULL, NULL }
};

static char *agsfh_readline(FILE *f)
{
    size_t cap = 8192, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (n == 0 && c == EOF) { free(buf); return NULL; }
    buf[n] = 0;
    while (n > 0 && buf[n - 1] == '\r') buf[--n] = 0; /* CRLF-aware */
    return buf;
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; } }
    char *line;
    while ((line = agsfh_readline(f)) != NULL) {
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

        agsf_fn fn = NULL;
        for (int i = 0; AGSF_DISPATCH[i].name; i++)
            if (strcmp(func, AGSF_DISPATCH[i].name) == 0) { fn = AGSF_DISPATCH[i].fn; break; }

        agsf_io_t io;
        io.seams = seams_node;
        io.cursors = json_object();
        io.trace = json_array();
        json_t *ret;
        if (fn) ret = fn(&io, argv_node, kw_node);
        else ret = agsf_mkraise("RuntimeError", "unknown func");

        json_t *out = json_object();
        json_set(out, "func", json_string(func));
        json_set(out, "in", in ? json_copy(in) : json_object());
        json_set(out, "ret", ret ? json_copy(ret) : json_null());
        json_set(out, "trace", io.trace);
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
