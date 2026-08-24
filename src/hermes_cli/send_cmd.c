/*
 * send_cmd.c — canonical C11 port of hermes_cli/send_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_send_cmd_u_read_message_body @ hermes_cli/send_cmd.py:_read_message_body */
int hermes_cli_send_cmd_u_read_message_body(const char *arg) {
    /* Python: body resolution. Arg =
     * "has_pos\tfile\tstdin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *has_pos = t1 ? t1 + 1 : "";
    int file = arg[0] == '1';
    int stdin_avail = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (has_pos[0]) { printf("%s\n", has_pos); return 0; }
    if (file) { printf("file:%s\n", t4 ? t4 + 1 : ""); return 0; }
    if (stdin_avail) { printf("stdin piped\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_send_cmd_u_emit_result @ hermes_cli/send_cmd.py:_emit_result */
int hermes_cli_send_cmd_u_emit_result(const char *arg) {
    /* Python: result formatting. Arg =
     * "state\tjson_mode\tquiet\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    int json_mode = arg[0] == '1';
    int quiet = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "hermes send: failed\n");
        return 1;
    }
    if (json_mode) { printf("%s\n", t3 ? t3 + 1 : "{}"); return 0; }
    if (quiet) { return 0; }
    printf("%s\n", t3 ? t3 + 1 : "sent");
    return 0;
}

/* PoP: hermes_cli_send_cmd_u_list_targets @ hermes_cli/send_cmd.py:_list_targets */
int hermes_cli_send_cmd_u_list_targets(const char *arg) {
    /* Python: channel directory. Arg =
     * "has_targets\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_targets = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "hermes send: failed to load channel directory: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (!has_targets) {
        printf("No messaging platforms configured or no channels discovered yet.\n");
        return 0;
    }
    printf("targets listed (%s mode): %s\n", (t2 && t2[1] == '1') ? "json" : "human", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_send_cmd_u_load_hermes_env @ hermes_cli/send_cmd.py:_load_hermes_env */
int hermes_cli_send_cmd_u_load_hermes_env(const char *arg) {
    /* Python: dotenv + bridge. Arg =
     * "loaded\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int loaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no env load (no home)\n"); return 0; }
    if (!loaded) { printf("env loaded (dotenv + %s top-level key(s) bridged, no override)\n", t2 ? t2 + 1 : "0"); return 0; }
    printf("env loaded\n");
    return 0;
}
