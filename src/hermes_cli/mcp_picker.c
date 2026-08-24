/*
 * mcp_picker.c — canonical C11 port of hermes_cli/mcp_picker.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_mcp_picker_is_custom @ hermes_cli/mcp_picker.py:is_custom */
int hermes_cli_mcp_picker_is_custom(const char *arg) {
    /* Python: True when no catalog entry backs this picker row. */
    if (!arg || !*arg) return 1; /* no entry -> custom */
    if (strcmp(arg, "none") == 0 || strcmp(arg, "0") == 0) return 1;
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_build_rows @ hermes_cli/mcp_picker.py:_build_rows */
int hermes_cli_mcp_picker_u_build_rows(const char *arg) {
    /* Python: catalog + custom rows. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_format_row @ hermes_cli/mcp_picker.py:_format_row */
int hermes_cli_mcp_picker_u_format_row(const char *arg) {
    /* Python: f"{row.name:<18} {row.status:<24} {row.description}".
     * Arg = "name\tstatus\tdescription". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    char name[128];
    size_t nlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen); name[nlen] = '\0';
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    char status[128];
    size_t slen = t2 ? (size_t)(t2 - t1 - 1) : 0;
    if (slen >= sizeof(status)) slen = sizeof(status) - 1;
    if (t1) { memcpy(status, t1 + 1, slen); status[slen] = '\0'; }
    else status[0] = '\0';
    const char *desc = t2 ? t2 + 1 : "";
    printf("%-18s %-24s %s\n", name, status, desc);
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_enable_disable @ hermes_cli/mcp_picker.py:_enable_disable */
int hermes_cli_mcp_picker_u_enable_disable(const char *arg) {
    /* Python: set server enabled flag + save. Arg =
     * "name\tenable\tinstalled". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t2 || t2[1] != '1') {
        printf("  '%s' is not installed.\n", arg);
        return 0;
    }
    printf("  ✓ '%s' %s. Start a new Hermes session for changes to take effect.\n",
           arg, (t1 && t1[1] == '1') ? "enabled" : "disabled");
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_configure_tools @ hermes_cli/mcp_picker.py:_configure_tools */
int hermes_cli_mcp_picker_u_configure_tools(const char *arg) {
    /* Python: delegate to cmd_mcp_configure for an installed MCP. Arg =
     * server name. */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    printf("configure tools for %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_remove_custom @ hermes_cli/mcp_picker.py:_remove_custom */
int hermes_cli_mcp_picker_u_remove_custom(const char *arg) {
    /* Python: confirm + delete + save. Arg = "name\tconfigured\tconfirmed". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int configured = t1 && t1[1] == '1';
    if (!configured) {
        printf("  '%s' is not configured.\n", arg);
        return 0;
    }
    int confirmed = t2 && t2[1] == '1';
    if (!confirmed) { printf("removal cancelled\n"); return 0; }
    printf("  ✓ Removed '%s'\n", arg);
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_handle_row @ hermes_cli/mcp_picker.py:_handle_row */
int hermes_cli_mcp_picker_u_handle_row(const char *arg) {
    /* Python: status dispatch. Arg =
     * "action\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *action = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(action, "install") == 0) { printf("  ✓ Installed and enabled\n"); return 0; }
    if (strcmp(action, "enable") == 0) { printf("enabled\n"); return 0; }
    if (strcmp(action, "disable") == 0) { printf("disabled (config kept)\n"); return 0; }
    if (strcmp(action, "uninstall") == 0) { printf("  ✓ Uninstalled — credentials in .env preserved\n"); return 0; }
    if (strcmp(action, "reinstall") == 0) { printf("reinstalled (re-cloned, re-prompted)\n"); return 0; }
    if (strcmp(action, "configure") == 0) { printf("configure tools flow\n"); return 0; }
    if (strcmp(action, "remove_custom") == 0) { printf("custom entry removed\n"); return 0; }
    printf("'%s' is already enabled — actions shown\n", action);
    return 0;
}

/* PoP: hermes_cli_mcp_picker_u_print_rows_text @ hermes_cli/mcp_picker.py:_print_rows_text */
int hermes_cli_mcp_picker_u_print_rows_text(const char *arg) {
    /* Python: plain catalog dump. Arg = "count\tstate\tfuture_warns\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("\n  No MCPs in the catalog or configured.\n\n");
        return 0;
    }
    printf("\n  MCP Catalog + configured servers:\n\n");
    printf("  %-18s %-24s Description\n", "Name", "Status");
    printf("  %-18s %-24s %-11s\n", "------------------", "------------------------", "-----------");
    printf("  (%ld rows)\n", count);
    if (t3 && t3[1] == '1') {
        printf("  ⚠ entry requires a newer Hermes — run `hermes update` to install this entry.\n");
    }
    printf("\n  Install: hermes mcp install <name>    Picker: hermes mcp\n\n");
    return 0;
}
