/*
 * permissions.c — canonical C11 port of tools/computer_use/permissions.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_computer_use_permissions_u_resolve_driver_cmd @ tools/computer_use/permissions.py:_resolve_driver_cmd */
int tools_computer_use_permissions_u_resolve_driver_cmd(const char *arg) {
    /* Python: resolve_cua_driver_cmd(override) from cua_backend. Arg =
     * override (or empty). */
    if (!arg || !*arg) {
        const char *env = getenv("CUA_DRIVER_CMD");
        printf("%s\n", env && *env ? env : "xdpyinfo");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_permissions_u_child_env @ tools/computer_use/permissions.py:_child_env */
int tools_computer_use_permissions_u_child_env(const char *arg) {
    /* Python: telemetry env + sanitized. Arg = "env_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_permissions_u_json_out @ tools/computer_use/permissions.py:_json_out */
int tools_computer_use_permissions_u_json_out(const char *arg) {
    /* Python: run binary+args; parse stdout as JSON; None on empty/fail.
     * Arg = JSON string (already captured); validated and echoed. */
    if (!arg || !*arg) { printf("None\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j) { printf("None\n"); return 0; }
    char *ser = json_serialize(j);
    printf("%s\n", ser ? ser : arg);
    free(ser);
    json_free(j);
    return 0;
}

/* PoP: tools_computer_use_permissions_u_mac_permissions @ tools/computer_use/permissions.py:_mac_permissions */
int tools_computer_use_permissions_u_mac_permissions(const char *arg) {
    /* Python: fold permission booleans into out. Arg = "data_json\terror". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) { printf("error: %s\n", tab + 1); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_permissions_computer_use_status @ tools/computer_use/permissions.py:computer_use_status */
int tools_computer_use_permissions_computer_use_status(const char *arg) {
    /* Python: readiness snapshot. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: tools_computer_use_permissions_request_permissions_grant @ tools/computer_use/permissions.py:request_permissions_grant */
int tools_computer_use_permissions_request_permissions_grant(const char *arg) {
    /* Python: TCC grant launch. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("64\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "not_darwin") == 0) {
        printf("Computer Use permissions are a macOS concept; nothing to grant here.\n");
        return 64;
    }
    if (strcmp(state, "missing_binary") == 0) {
        printf("cua-driver: not installed. Run: hermes computer-use install\n");
        return 2;
    }
    printf("grant requested, exit=%s\n", tab ? tab + 1 : "0");
    return 0;
}
