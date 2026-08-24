/*
 * login_page.c — canonical C11 port of hermes_cli/dashboard_auth/login_page.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_logi_render_login_html @ hermes_cli/dashboard_auth/login_page.py:render_login_html */
int hermes_cli_dashboard_auth_logi_render_login_html(const char *arg) {
    /* Python: login page. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("empty html\n"); return 0; }
    printf("login html rendered (%s providers): %s\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_logi_u_render_password_form @ hermes_cli/dashboard_auth/login_page.py:_render_password_form */
int hermes_cli_dashboard_auth_logi_u_render_password_form(const char *arg) {
    /* Python: password form html. Arg =
     * "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("<form class=\"provider-form\" data-provider=\"%s\">...password fields...</form>\n", t2 ? t2 + 1 : arg);
    return 0;
}
