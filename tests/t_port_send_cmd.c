/*
 * t_port_send_cmd.c -- oracle harness for port_send_cmd.c
 */
#include "port_send_cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *label, const char *json, int json_mode, int quiet)
{
    int err = 0;
    char *text = NULL;
    int display = sc_display(json, json_mode, quiet, &text, &err);
    int code = sc_exit_code(json);

    printf("{\"fn\":\"_emit_result\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"json_mode\":%s,\"quiet\":%s,", json_mode ? "true" : "false",
           quiet ? "true" : "false");
    printf("\"exit\":%d,\"display\":%d,\"to_stderr\":%s,\"text\":",
           code, display, err ? "true" : "false");
    if (text) {
        putchar('"');
        for (char *q = text; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    } else {
        fputs("null", stdout);
    }
    printf("}\n");
    free(text);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *ok_note   = "{\"success\":true,\"note\":\"delivered to telegram\"}";
    const char *ok_nonote = "{\"success\":true}";
    const char *ok_skip   = "{\"skipped\":true,\"reason\":\"no target\"}";
    const char *err        = "{\"error\":\"platform not configured\"}";
    const char *err_empty  = "{\"error\":\"\",\"success\":true}";
    const char *unknown    = "{\"something\":\"else\"}";
    const char *bad        = "{not json";
    const char *empty      = "";
    const char *null_json  = NULL;
    const char *err_and_ok = "{\"error\":\"boom\",\"success\":true}";

    /* quiet=false, json_mode=false -> the real branch decisions */
    emit("ok-note",   ok_note,   0, 0);
    emit("ok-nonote", ok_nonote, 0, 0);
    emit("ok-skip",   ok_skip,   0, 0);
    emit("error",     err,       0, 0);
    emit("err-empty", err_empty, 0, 0);
    emit("unknown",   unknown,   0, 0);
    emit("bad-json",  bad,       0, 0);
    emit("empty",     empty,     0, 0);
    emit("null",      null_json, 0, 0);
    emit("err-and-ok", err_and_ok, 0, 0);

    /* quiet=true suppresses */
    emit("ok-quiet",   ok_note, 0, 1);
    emit("err-quiet",  err,     0, 1);

    /* json_mode overrides quiet */
    emit("ok-json",    ok_note, 1, 0);
    emit("err-json",   err,     1, 0);
    emit("ok-json-q",  ok_note, 1, 1);

    return 0;
}
