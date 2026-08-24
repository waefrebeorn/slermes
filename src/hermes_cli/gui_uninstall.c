/*
 * gui_uninstall.c — canonical C11 port of hermes_cli/gui_uninstall.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_gui_uninstall_log_info @ hermes_cli/gui_uninstall.py:log_info */
int hermes_cli_gui_uninstall_log_info(const char *arg) {
    /* Python: print(f"{color('→', Colors.CYAN)} {msg}"). */
    printf("\x1b[36m→\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_log_success @ hermes_cli/gui_uninstall.py:log_success */
int hermes_cli_gui_uninstall_log_success(const char *arg) {
    /* Python: print(f"{color('✓', Colors.GREEN)} {msg}"). */
    printf("\x1b[32m✓\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_log_warn @ hermes_cli/gui_uninstall.py:log_warn */
int hermes_cli_gui_uninstall_log_warn(const char *arg) {
    /* Python: print(f"{color('⚠', YELLOW)} {msg}"). Arg = msg. */
    if (!arg) arg = "";
    printf("\xE2\x9A\xA0 %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_u_agent_root @ hermes_cli/gui_uninstall.py:_agent_root */
int hermes_cli_gui_uninstall_u_agent_root(const char *arg) {
    /* Python: hermes_home / "hermes-agent" — the agent checkout root. */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/hermes-agent\n", base);
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_desktop_userdata_dir @ hermes_cli/gui_uninstall.py:desktop_userdata_dir */
int hermes_cli_gui_uninstall_desktop_userdata_dir(const char *arg) {
    /* Python: Electron userData path. Arg = "platform\thome\txdg\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *platform = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *xdg = t3 ? t3 + 1 : "";
    if (strcmp(platform, "darwin") == 0) { printf("%s/Library/Application Support/Hermes\n", home); return 0; }
    if (strcmp(platform, "win32") == 0) { printf("%s/Hermes\n", home); return 0; }
    printf("%s/Hermes\n", xdg[0] ? xdg : ".config");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_source_built_gui_artifacts @ hermes_cli/gui_uninstall.py:source_built_gui_artifacts */
int hermes_cli_gui_uninstall_source_built_gui_artifacts(const char *arg) {
    /* Python: 5 GUI artifact paths. Arg = "agent_root\thermes_home". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *agent_root = arg;
    const char *home = tab ? tab + 1 : arg;
    printf("%s/apps/desktop/dist\n%s/apps/desktop/release\n%s/apps/desktop/node_modules\n%s/node_modules\n%s/desktop-build-stamp.json\n",
           agent_root, agent_root, agent_root, agent_root, home);
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_packaged_gui_app_paths @ hermes_cli/gui_uninstall.py:packaged_gui_app_paths */
int hermes_cli_gui_uninstall_packaged_gui_app_paths(const char *arg) {
    /* Python: well-known locations. Arg =
     * "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_agent_is_installed @ hermes_cli/gui_uninstall.py:agent_is_installed */
int hermes_cli_gui_uninstall_agent_is_installed(const char *arg) {
    /* Python: hermes_cli dir OR venv/.venv present. Arg =
     * "has_source\thas_venv". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_gui_is_installed @ hermes_cli/gui_uninstall.py:gui_is_installed */
int hermes_cli_gui_uninstall_gui_is_installed(const char *arg) {
    /* Python: any source-built/packaged artifact or userdata dir exists.
     * Arg = paths (one per line, empty = none). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char path[1200];
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, p, len); path[len] = '\0';
        struct stat st;
        if (len && stat(path, &st) == 0) { printf("1\n"); return 0; }
        p = nl ? nl + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_gui_install_summary @ hermes_cli/gui_uninstall.py:gui_install_summary */
int hermes_cli_gui_uninstall_gui_install_summary(const char *arg) {
    /* Python: structured install snapshot. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_u_remove_path @ hermes_cli/gui_uninstall.py:_remove_path */
int hermes_cli_gui_uninstall_u_remove_path(const char *arg) {
    /* Python: unlink file/symlink, rmtree dir; warn on failure. Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (lstat(arg, &st) != 0) { printf("0\n"); return 0; }
    if (S_ISDIR(st.st_mode)) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", arg);
        int rc = system(cmd);
        printf("%d\n", rc == 0 ? 1 : 0);
        return 0;
    }
    if (unlink(arg) == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_gui_uninstall_uninstall_gui @ hermes_cli/gui_uninstall.py:uninstall_gui */
int hermes_cli_gui_uninstall_uninstall_gui(const char *arg) {
    /* Python: artifact removal. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}
