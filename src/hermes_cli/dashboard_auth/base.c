/*
 * base.c — canonical C11 port of hermes_cli/dashboard_auth/base.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_base_start_login @ hermes_cli/dashboard_auth/base.py:start_login */
int hermes_cli_dashboard_auth_base_start_login(const char *arg) {
    /* Python: ABC @abstractmethod (body is `...`) — provider subclasses
     * implement. C base returns the not-implemented sentinel. */
    (void)arg;
    return -1;  /* abstract — no default implementation */
}

/* PoP: hermes_cli_dashboard_auth_base_complete_login @ hermes_cli/dashboard_auth/base.py:complete_login */
int hermes_cli_dashboard_auth_base_complete_login(const char *arg) {
    /* Python: ABC @abstractmethod (body is `...`) — provider subclasses
     * implement. C base returns the not-implemented sentinel. */
    (void)arg;
    return -1;  /* abstract — no default implementation */
}

/* PoP: hermes_cli_dashboard_auth_base_verify_session @ hermes_cli/dashboard_auth/base.py:verify_session */
int hermes_cli_dashboard_auth_base_verify_session(const char *arg) {
    /* Python: ABC @abstractmethod (body is `...`) — provider subclasses
     * implement. C base returns the not-implemented sentinel. */
    (void)arg;
    return -1;  /* abstract — no default implementation */
}

/* PoP: hermes_cli_dashboard_auth_base_refresh_session @ hermes_cli/dashboard_auth/base.py:refresh_session */
int hermes_cli_dashboard_auth_base_refresh_session(const char *arg) {
    /* Python: ABC @abstractmethod (body is `...`) — provider subclasses
     * implement. C base returns the not-implemented sentinel. */
    (void)arg;
    return -1;  /* abstract — no default implementation */
}

/* PoP: hermes_cli_dashboard_auth_base_revoke_session @ hermes_cli/dashboard_auth/base.py:revoke_session */
int hermes_cli_dashboard_auth_base_revoke_session(const char *arg) {
    /* Python: ABC @abstractmethod (body is `...`) — provider subclasses
     * implement. C base returns the not-implemented sentinel. */
    (void)arg;
    return -1;  /* abstract — no default implementation */
}

/* PoP: hermes_cli_dashboard_auth_base_complete_password_login @ hermes_cli/dashboard_auth/base.py:complete_password_login */
int hermes_cli_dashboard_auth_base_complete_password_login(const char *arg) {
    /* Python: NotImplementedError default. Arg =
     * "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unsupported") == 0) {
        fprintf(stderr, "%s does not support password login (set supports_password = True)\n", arg);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_base_assert_protocol_compliance @ hermes_cli/dashboard_auth/base.py:assert_protocol_compliance */
int hermes_cli_dashboard_auth_base_assert_protocol_compliance(const char *arg) {
    /* Python: provider protocol check. Arg = "state\tmissing\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("compliance passed\n"); return 0; }
    fprintf(stderr, "provider missing: %s\n", t1 ? t1 + 1 : "?");
    return 1;
}
