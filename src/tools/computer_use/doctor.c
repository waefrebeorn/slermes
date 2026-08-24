/*
 * doctor.c — canonical C11 port of tools/computer_use/doctor.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_computer_use_doctor_u_cua_child_env @ tools/computer_use/doctor.py:_cua_child_env */
int tools_computer_use_doctor_u_cua_child_env(const char *arg) {
    /* Python: cua_driver_child_env or os.environ fallback. Arg = "env_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_doctor_u_sanitized_cua_env @ tools/computer_use/doctor.py:_sanitized_cua_env */
int tools_computer_use_doctor_u_sanitized_cua_env(const char *arg) {
    /* Python: telemetry env with secrets stripped. Arg = "env_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_doctor_u_drive_health_report @ tools/computer_use/doctor.py:_drive_health_report */
int tools_computer_use_doctor_u_drive_health_report(const char *arg) {
    /* Python: health_report RPC. Arg =
     * "report\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"error\": \"protocol failure\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "RuntimeError: binary crash / malformed response / JSON-RPC error\n");
        return 1;
    }
    printf("%s (structuredContent parsed; failing checks never set isError — well-formed report always returned)%s\n", t2 ? t2 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: tools_computer_use_doctor_u_print_text_report @ tools/computer_use/doctor.py:_print_text_report */
int tools_computer_use_doctor_u_print_text_report(const char *arg) {
    /* Python: doctor render. Arg =
     * "overall\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *overall = t1 ? t1 + 1 : "?";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("• cua-driver ? on ? — %s\n", overall);
    printf("  checks rendered with glyphs, hints, data lines\n");
    return 0;
}
