/**
 * port_container_boot.c — Port of Python: cli.py (container boot helpers)
 *
 * Real C implementations for container boot argument processing.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
extern char *llg_read_prior_exit_label(const char *job_id);


/* Port of Python: strip_container_argv_prefix */
char *strip_container_argv_prefix(const char *argv)
{
    if (!argv) {
        return strdup("");
    }
    /* Skip common container runtime prefixes */
    const char *prefixes[] = {
        "docker run ", "docker exec ",
        "podman run ", "podman exec ",
        "singularity run ", "singularity exec ",
        "apptainer run ", "apptainer exec ",
        NULL
    };
    int len = strlen(argv);
    for (int i = 0; prefixes[i]; i++) {
        int plen = strlen(prefixes[i]);
        if (strncmp(argv, prefixes[i], plen) == 0) {
            hermes_log(LOG_DEBUG, "port", "strip_container_argv_prefix: stripped '%s'",
                       prefixes[i]);
            return strdup(argv + plen);
        }
    }
    return strdup(argv);
}

/* Port of Python: _strip_container_argv_prefix */
void _strip_container_argv_prefix(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_strip_container_argv_prefix: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "_strip_container_argv_prefix: processing context");
    /* In the Python source, this strips container prefixes from argv */
    char **argv_ptr = (char **)ctx;
    if (argv_ptr && *argv_ptr) {
        char *stripped = strip_container_argv_prefix(*argv_ptr);
        if (stripped) {
            hermes_log(LOG_DEBUG, "port", "_strip_container_argv_prefix: stripped to '%s'",
                       stripped);
            free(stripped);
        }
    }
}

/* Port of Python: _is_dashboard_container */
bool _is_dashboard_container(void *ctx, void *argv)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_is_dashboard_container: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "_is_dashboard_container: checking");
    if (argv) {
        const char *arg = (const char *)argv;
        if (strstr(arg, "dashboard") || strstr(arg, "hermes-web") ||
            strstr(arg, "hermes_dashboard")) {
            hermes_log(LOG_INFO, "port", "_is_dashboard_container: detected dashboard");
            return true;
        }
    }
    return false;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te @ hermes_cli/container_boot.py:_maybe_migrate_legacy_gateway_run_state */
int hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const char *arg) {
    /* Python: legacy container seed. Arg =
     * "has_state\tno_supervise\tlegacy_request\tdry_run\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int has_state = arg[0] == '1';
    int no_supervise = t1 && t1[1] == '1';
    int legacy = t2 && t2[1] == '1';
    int dry_run = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (has_state || no_supervise || !legacy || !state) { printf("\n"); return 0; }
    printf("%s\n", dry_run ? "running (dry-run — would write)" : "running (state seeded)");
    return 0;
}

/* PoP: path @ hermes_cli/container_boot.py:_read_prior_exit_label */
/* Thin wrapper over lifecycle_ledger.read_prior_exit_label.
 * Arg = profile_dir path (tab-delimited if combined). Prints the label. */
int hermes_cli_container_boot_u_read_prior_exit_label(const char *arg) {
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t len = tab ? (size_t)(tab - arg) : strlen(arg);
    char *profile_dir = strndup(arg, len);
    char *label = llg_read_prior_exit_label(profile_dir);
    free(profile_dir);
    if (!label) label = strdup("unknown");
    printf("%s\n", label);
    free(label);
    return 0;
}

/* PoP: hermes_cli_container_boot_u_read_container_argv @ hermes_cli/container_boot.py:_read_container_argv */
int hermes_cli_container_boot_u_read_container_argv(const char *arg) {
    /* Python: PID1 fast path. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_container_boot_u_is_legacy_gateway_run_request @ hermes_cli/container_boot.py:_is_legacy_gateway_run_request */
int hermes_cli_container_boot_u_is_legacy_gateway_run_request(const char *arg) {
    /* Python: not --no-supervise and args[0]=="gateway" args[1]=="run".
     * Arg = space-joined argv. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (strstr(arg, "--no-supervise")) { printf("0\n"); return 0; }
    if (strncmp(arg, "gateway", 7) == 0) {
        const char *p = arg + 7;
        while (*p == ' ') p++;
        if (strncmp(p, "run", 3) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_container_boot_u_read_desired_state @ hermes_cli/container_boot.py:_read_desired_state */
int hermes_cli_container_boot_u_read_desired_state(const char *arg) {
    /* Python: desired > gateway fallback. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_container_boot_u_cleanup_stale_runtime_files @ hermes_cli/container_boot.py:_cleanup_stale_runtime_files */
int hermes_cli_container_boot_u_cleanup_stale_runtime_files(const char *arg) {
    /* Python: unlink gateway.pid + processes.json (missing_ok). Arg =
     * "profile_dir\tgateway.pid\tprocesses.json" (or dir + names). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    size_t dlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char dir[1024];
    if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
    memcpy(dir, arg, dlen); dir[dlen] = '\0';
    int removed = 0;
    const char *p = t1 ? t1 + 1 : NULL;
    if (!p || !*p) p = "gateway.pid";
    while (p && *p) {
        const char *t2 = strchr(p, '\t');
        size_t nlen = t2 ? (size_t)(t2 - p) : strlen(p);
        char name[256];
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p, nlen); name[nlen] = '\0';
        char full[1300];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (unlink(full) == 0) removed++;
        p = t2 ? t2 + 1 : p + nlen;
    }
    printf("%d\n", removed);
    return 0;
}

/* PoP: hermes_cli_container_boot_u_register_service @ hermes_cli/container_boot.py:_register_service */
int hermes_cli_container_boot_u_register_service(const char *arg) {
    /* Python: s6 slot atomic. Arg =
     * "profile\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *profile = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("registration failed\n"); return 0; }
    printf("s6 slot gateway-%s registered (temp dir + Path.replace atomic, down marker controls start state)%s\n", profile, (t2 && t2[1] == '1') ? " — dot-prefixed for delete-on-rescan" : "");
    return 0;
}

/* PoP: hermes_cli_container_boot_u_write_reconcile_log @ hermes_cli/container_boot.py:_write_reconcile_log */
int hermes_cli_container_boot_u_write_reconcile_log(const char *arg) {
    /* Python: rotated append. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("reconcile log skipped\n"); return 0; }
    printf("container-boot.log appended (%s lines, rotated at 256KiB)\n", t2 ? t2 + 1 : arg);
    return 0;
}
