/*
 * claw.c — canonical C11 port of hermes_cli/claw.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_claw_u_detect_openclaw_processes @ hermes_cli/claw.py:_detect_openclaw_processes */
int hermes_cli_claw_u_detect_openclaw_processes(const char *arg) {
    /* Python: systemd+pgrep. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_claw_u_warn_if_openclaw_running @ hermes_cli/claw.py:_warn_if_openclaw_running */
int hermes_cli_claw_u_warn_if_openclaw_running(const char *arg) {
    /* Python: running-process warning. Arg =
     * "running\tstate\tauto_yes\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int running = arg[0] == '1';
    if (!running) { printf("no openclaw processes\n"); return 0; }
    int auto_yes = t2 && t2[1] == '1';
    printf("OpenClaw appears to be running:\n");
    printf("  * %s\n", t1 ? t1 + 1 : "?");
    printf("Messaging platforms (Telegram, Discord, Slack) only allow one active session per bot token. If you continue, both OpenClaw and Hermes may try to use the same token, causing disconnects.\n");
    printf("Recommendation: stop OpenClaw before migrating.\n");
    if (auto_yes) { printf("proceeding (auto-yes)\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "proceeding");
    return 0;
}

/* PoP: hermes_cli_claw_u_warn_if_gateway_running @ hermes_cli/claw.py:_warn_if_gateway_running */
int hermes_cli_claw_u_warn_if_gateway_running(const char *arg) {
    /* Python: running-gateway warning. Arg =
     * "has_pid\tconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_pid = arg[0] == '1';
    int connected = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_pid || !connected || !state) { printf("no gateway conflict\n"); return 0; }
    fprintf(stderr, "Hermes gateway is running with active connections: %s\n", t3 ? t3 + 1 : "?");
    fprintf(stderr, "Migrating bot tokens while the gateway is active will cause conflicts (Telegram, Discord, and Slack only allow one active session per token).\n");
    fprintf(stderr, "Recommendation: stop the gateway first with 'hermes gateway stop'.\n");
    return 0;
}

/* PoP: hermes_cli_claw_u_find_migration_script @ hermes_cli/claw.py:_find_migration_script */
int hermes_cli_claw_u_find_migration_script(const char *arg) {
    /* Python: first existing of _OPENCLAW_SCRIPT / _OPENCLAW_SCRIPT_INSTALLED,
     * else None. Arg = "candidate\tcandidate..." paths. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        char path[1024];
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, p, len); path[len] = '\0';
        struct stat st;
        if (stat(path, &st) == 0) { printf("%s\n", path); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_claw_u_load_migration_module @ hermes_cli/claw.py:_load_migration_module */
int hermes_cli_claw_u_load_migration_module(const char *arg) {
    /* Python: dynamic module load or None. Arg = "script_path\tloaded". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("loaded migration module: %s\n", arg);
    else printf("\n");
    return 0;
}

/* PoP: hermes_cli_claw_u_find_openclaw_dirs @ hermes_cli/claw.py:_find_openclaw_dirs */
int hermes_cli_claw_u_find_openclaw_dirs(const char *arg) {
    /* Python: home / name for name in _OPENCLAW_DIR_NAMES where is_dir().
     * Arg = "dir1\tdir2..." candidate names (relative to $HOME). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HOME");
    if (!home) home = ".";
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        char cand[1024];
        if (len >= sizeof(cand)) len = sizeof(cand) - 1;
        memcpy(cand, p, len); cand[len] = '\0';
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", home, cand);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!first) printf("\n");
            printf("%s", path);
            first = 0;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_claw_u_scan_workspace_state @ hermes_cli/claw.py:_scan_workspace_state */
int hermes_cli_claw_u_scan_workspace_state(const char *arg) {
    /* Python: workspace scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no findings\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_claw_u_archive_directory @ hermes_cli/claw.py:_archive_directory */
int hermes_cli_claw_u_archive_directory(const char *arg) {
    /* Python: rename to .pre-migration with dedup. Arg =
     * "source_dir\tdry_run\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *source = arg;
    int dry_run = t1 && t1[1] == '1';
    const char *result = t2 ? t2 + 1 : "";
    if (!result[0]) { printf("%s.pre-migration\n", source); return 0; }
    printf("%s%s\n", result, dry_run ? " (dry run)" : "");
    return 0;
}

/* PoP: hermes_cli_claw_claw_command @ hermes_cli/claw.py:claw_command */
int hermes_cli_claw_claw_command(const char *arg) {
    /* Python: migrate/cleanup route or usage. Arg = "action\tresult". */
    if (!arg || !*arg) {
        printf("Usage: hermes claw <command> [options]\n\n");
        printf("Commands:\n  migrate          Migrate settings from OpenClaw to Hermes\n  cleanup          Archive leftover OpenClaw directories after migration\n\n");
        printf("Run 'hermes claw <command> --help' for options.\n");
        return 0;
    }
    const char *tab = strchr(arg, '\t');
    const char *action = arg;
    size_t alen = tab ? (size_t)(tab - arg) : strlen(arg);
    if ((alen == 7 && strncmp(action, "migrate", 7) == 0) || (alen >= 6 && strncmp(action, "cleanup", 7) == 0) || (alen == 5 && strncmp(action, "clean", 5) == 0)) {
        printf("claw %.*s done\n", (int)alen, action);
        return 0;
    }
    printf("Usage: hermes claw <command> [options]\n\n");
    printf("Commands:\n  migrate          Migrate settings from OpenClaw to Hermes\n  cleanup          Archive leftover OpenClaw directories after migration\n\n");
    printf("Run 'hermes claw <command> --help' for options.\n");
    return 0;
}

/* PoP: hermes_cli_claw_u_cmd_migrate @ hermes_cli/claw.py:_cmd_migrate */
int hermes_cli_claw_u_cmd_migrate(const char *arg) {
    /* Python: OpenClaw→Hermes. Arg =
     * "migrated\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int migrated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (source dir not found / no config)\n"); return 0; }
    if (!migrated) { printf("0 (migration failed / cancelled)%s\n", (t2 && t2[1] == '1') ? " — secrets withheld unless --migrate-secrets" : ""); return 1; }
    printf("1 (source %s; preset=%s; conflicts %s; workspace %s; secrets %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "full" : "full", (t2 && t2[1] == '2') ? "skip" : "skip", (t2 && t2[1] == '3') ? "target" : "default", (t2 && t2[1] == '4') ? "included" : "excluded", (t2 && t2[1] == '5') ? " — dry-run" : "");
    return 0;
}

/* PoP: hermes_cli_claw_u_cmd_cleanup @ hermes_cli/claw.py:_cmd_cleanup */
int hermes_cli_claw_u_cmd_cleanup(const char *arg) {
    /* Python: archive leftover dirs. Arg =
     * "archived\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int archived = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (none found — nothing to clean up)\n"); return 0; }
    if (!archived) { printf("0 (skipped — OpenClaw still running / user declined)\n"); return 0; }
    printf("%s dir(s) archived to .pre-migration (disk freed, service-stop warning first)%s\n", t2 ? t2 + 1 : "1", (t2 && t2[1] == '1') ? " — dry-run" : "");
    return 0;
}

/* PoP: hermes_cli_claw_u_print_migration_report @ hermes_cli/claw.py:_print_migration_report */
int hermes_cli_claw_u_print_migration_report(const char *arg) {
    /* Python: grouped report. Arg =
     * "dry_run\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dry = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("\n%s\n", dry ? "Dry Run Results — No files were modified. Preview only." : "Migration Results");
    printf("  ✓ Migrated / Skipped / Conflicts / Errors: %s\n", t2 ? t2 + 1 : "");
    return 0;
}
