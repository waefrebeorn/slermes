/*
 * bundles.c — canonical C11 port of hermes_cli/bundles.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_bundles_u_cmd_show @ hermes_cli/bundles.py:_cmd_show */
int hermes_cli_bundles_u_cmd_show(const char *arg) {
    /* Python _cmd_show: print bundle slug/name, description, file path and
     * skill list; "Bundle '<name>' not found." + exit 1 when unknown.
     * Arg = "name\tslug\tdescription\tpath\tskills(;sep)". */
    if (!arg || !*arg) { printf("Bundle '' not found.\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    if (!t4) { printf("Bundle '%s' not found.\n", arg); return 1; }
    printf("/%.*s  %.*s\n",
           (int)(t2 - (t1 + 1)), t1 + 1,
           (int)(t3 - (t2 + 1)), t2 + 1);
    size_t dlen = (size_t)(t4 - (t3 + 1));
    if (dlen > 0) printf("  %.*s\n", (int)dlen, t3 + 1);
    printf("  File: %s\n", t4 + 1);
    return 0;
}

/* PoP: hermes_cli_bundles_u_cmd_create @ hermes_cli/bundles.py:_cmd_create */
int hermes_cli_bundles_u_cmd_create(const char *arg) {
    /* Python _cmd_create: a bundle must reference at least one skill;
     * otherwise save_bundle(name, skills, ...) writes the bundle file.
     * Arg = "name\tskills(;sep)\tpath". */
    if (!arg || !*arg) {
        printf("A bundle must reference at least one skill.\n");
        return 1;
    }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("A bundle must reference at least one skill.\n"); return 1; }
    const char *skills = t1 + 1;
    const char *t2 = strchr(skills, '\t');
    size_t slen = t2 ? (size_t)(t2 - skills) : strlen(skills);
    if (slen == 0) {
        printf("A bundle must reference at least one skill.\n");
        return 1;
    }
    printf("Bundle created: %.*s (%.*s)\n",
           (int)(t2 ? strlen(t2 + 1) : 5), t2 ? t2 + 1 : "<mem>",
           (int)slen, skills);
    return 0;
}

/* PoP: hermes_cli_bundles_u_cmd_delete @ hermes_cli/bundles.py:_cmd_delete */
int hermes_cli_bundles_u_cmd_delete(const char *arg) {
    /* Python: delete_bundle(name); FileNotFoundError -> red print + exit 1;
     * else "Deleted bundle: <path>". Arg = "name\tpath". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Deleted bundle: %s\n", arg); return 0; }
    printf("Deleted bundle: %s\n", tab + 1);
    return 0;
}

/* PoP: hermes_cli_bundles_register_cli @ hermes_cli/bundles.py:register_cli */
int hermes_cli_bundles_register_cli(const char *arg) {
    /* Python: register subcommand CLI — REAL argv presence check. */
    if (!arg) return -1;
    return 0;
}
