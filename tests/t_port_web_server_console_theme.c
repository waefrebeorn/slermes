/*
 * t_port_web_server_console_theme.c — oracle harness for the console frame
 * protocol + theme cluster. Ops:
 *   {"op":"json_payload","text":...} | {"op":"json_payload","bytes_b64":...}
 *   {"op":"profile","q":"..."}
 *   {"op":"send_result","status":..,"command":..,"output":..,"confirm_msg":..,"id":N}
 *   {"op":"frames","script":[frame,frame,...]}   (session dispatch trace)
 *   {"op":"parse_layer","value":...,"hex":"...","alpha":N}
 *   {"op":"normalise","data":{...}}
 *   {"op":"builtins"}
 *   {"op":"discover","home":"..."}
 *   {"op":"themes_response","home":"...","active":"..."}
 *   {"op":"bootstrap_css","home":"...","active":"..."}
 *   {"op":"font_allowed","id":"..."}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_console.h"
#include "web_server_themes.h"
#include "libbase64/base64.h"

static void pjs(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc(*p, stdout);
        }
    }
}

/* scripted executor: fixture supplies results keyed by line */
static json_t *g_exec_table;
static bool scripted_exec(void *ctx, const char *line, bool confirmed,
                          ws_console_result_t *out) {
    (void)ctx;
    json_t *spec = g_exec_table ? json_object_get(g_exec_table, line) : NULL;
    if (!spec) {
        out->output = "no such command";
        return false;
    }
    (void)confirmed;
    out->status = json_get_str(spec, "status", "ok");
    out->command = json_get_str(spec, "command", line);
    out->output = json_get_str(spec, "output", NULL);
    out->confirmation_message = json_get_str(spec, "confirm_msg", NULL);
    return json_get_bool(spec, "ok", true);
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[131072];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");

    if (strcmp(op, "json_payload") == 0) {
        char *err = NULL;
        json_t *payload = NULL;
        const char *b64 = json_get_str(fx, "bytes_b64", NULL);
        if (b64) {
            size_t blen = 0;
            unsigned char *bytes = base64_decode(b64, &blen);
            payload = ws_console_json_payload(NULL, bytes, blen, &err);
            free(bytes);
        } else {
            payload = ws_console_json_payload(
                json_get_str(fx, "text", NULL), NULL, 0, &err);
        }
        printf("{\"payload\":");
        if (payload) { char *d = json_dumps(payload, 0); fputs(d, stdout); free(d); }
        else fputs("null", stdout);
        printf(",\"error\":");
        if (err) { printf("\""); pjs(err); printf("\""); }
        else fputs("null", stdout);
        printf("}\n");
        json_free(payload);
        free(err);
    } else if (strcmp(op, "profile") == 0) {
        char *r = ws_console_profile_from_query(json_get_str(fx, "q", NULL));
        if (r) { printf("{\"profile\":\""); pjs(r); printf("\"}\n"); free(r); }
        else printf("{\"profile\":null}\n");
    } else if (strcmp(op, "send_result") == 0) {
        ws_console_result_t r = {
            .status = json_get_str(fx, "status", ""),
            .command = json_get_str(fx, "command", NULL),
            .output = json_get_str(fx, "output", NULL),
            .confirmation_message = json_get_str(fx, "confirm_msg", NULL),
        };
        json_t *frames = json_array();
        ws_console_send_result(frames, &r, (int)json_get_num(fx, "id", 1));
        char *d = json_dumps(frames, 0);
        printf("%s\n", d);
        free(d);
        json_free(frames);
    } else if (strcmp(op, "frames") == 0) {
        g_exec_table = json_object_get(fx, "exec");
        ws_console_session_t *s = ws_console_session_new();
        json_t *script = json_object_get(fx, "script");
        json_t *trace = json_array();
        for (size_t i = 0; i < json_len(script); i++) {
            json_t *step = json_get(script, i);
            /* optional busy toggle before dispatch */
            json_t *busy = json_object_get(step, "_busy");
            if (busy) {
                ws_console_session_set_busy(s, busy->bool_val);
                continue;
            }
            json_t *frames = json_array();
            bool close = ws_console_handle_frame(s, step, scripted_exec, NULL,
                                                 frames);
            json_t *entry = json_object();
            json_set(entry, "frames", frames);
            json_set(entry, "close", json_bool(close));
            json_append(trace, entry);
        }
        char *d = json_dumps(trace, 0);
        printf("%s\n", d);
        free(d);
        json_free(trace);
        ws_console_session_free(s);
    } else if (strcmp(op, "parse_layer") == 0) {
        json_t *value = json_object_get(fx, "value");
        json_t *r = ws_theme_parse_layer(value, json_get_str(fx, "hex", "#000"),
                                         json_get_num(fx, "alpha", 1.0));
        if (r) { char *d = json_dumps(r, 0); printf("%s\n", d); free(d); json_free(r); }
        else printf("null\n");
    } else if (strcmp(op, "normalise") == 0) {
        json_t *r = ws_theme_normalise_definition(json_object_get(fx, "data"));
        if (r) { char *d = json_dumps(r, 0); printf("%s\n", d); free(d); json_free(r); }
        else printf("null\n");
    } else if (strcmp(op, "builtins") == 0) {
        json_t *r = ws_theme_builtin_list();
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else if (strcmp(op, "discover") == 0) {
        json_t *r = ws_theme_discover_user_themes(json_get_str(fx, "home", NULL));
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else if (strcmp(op, "themes_response") == 0) {
        json_t *r = ws_theme_dashboard_themes_response(
            json_get_str(fx, "home", NULL), json_get_str(fx, "active", NULL));
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else if (strcmp(op, "bootstrap_css") == 0) {
        char *r = ws_theme_render_bootstrap_css(
            json_get_str(fx, "home", NULL), json_get_str(fx, "active", NULL));
        printf("{\"css\":\"");
        pjs(r);
        printf("\"}\n");
        free(r);
    } else if (strcmp(op, "font_allowed") == 0) {
        printf("{\"allowed\":%s}\n",
               ws_theme_font_choice_allowed(json_get_str(fx, "id", NULL))
                   ? "true" : "false");
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
