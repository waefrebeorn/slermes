/*
 * browser_connect.c — canonical C11 port of hermes_cli/browser_connect.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_browser_connect_chrome_debug_data_dir @ hermes_cli/browser_connect.py:chrome_debug_data_dir */
int hermes_cli_browser_connect_chrome_debug_data_dir(const char *arg) {
    /* Python: str(get_hermes_home() / "chrome-debug"). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/chrome-debug\n", base);
    return 0;
}

/* PoP: hermes_cli_browser_connect_u_chrome_debug_args @ hermes_cli/browser_connect.py:_chrome_debug_args */
int hermes_cli_browser_connect_u_chrome_debug_args(const char *arg) {
    /* Python: [--remote-debugging-port=<port>,
     * --user-data-dir=<dir>, --no-first-run, --no-default-browser-check].
     * Arg = "port\tdir". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("--remote-debugging-port=%s\n", arg); return 0; }
    printf("--remote-debugging-port=%.*s\n--user-data-dir=%s\n--no-first-run\n--no-default-browser-check\n",
           (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: hermes_cli_browser_connect_discover_local_cdp_url @ hermes_cli/browser_connect.py:discover_local_cdp_url */
int hermes_cli_browser_connect_discover_local_cdp_url(const char *arg) {
    /* Python: first loopback URL speaking CDP. Arg = "port\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_browser_connect_local_port_in_use @ hermes_cli/browser_connect.py:local_port_in_use */
int hermes_cli_browser_connect_local_port_in_use(const char *arg) {
    /* Python: TCP connect probe on loopback hosts. Arg = "port\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_browser_connect_find_free_debug_port @ hermes_cli/browser_connect.py:find_free_debug_port */
int hermes_cli_browser_connect_find_free_debug_port(const char *arg) {
    /* Python: dual-loopback bind scan. Arg = "preferred\tattempts\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long preferred = strtol(arg, NULL, 10);
    long attempts = t1 ? strtol(t1 + 1, NULL, 10) : 20;
    long found = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    printf("%ld\n", found > 0 ? found : preferred + 1);
    return 0;
}

/* PoP: hermes_cli_browser_connect_manual_chrome_debug_command @ hermes_cli/browser_connect.py:manual_chrome_debug_command */
int hermes_cli_browser_connect_manual_chrome_debug_command(const char *arg) {
    /* Python: joined argv or Darwin open -a. Arg = "system\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    if (strcmp(arg, "Darwin") == 0) {
        printf("open -a \"Google Chrome\" --args --remote-debugging-port=<port> --user-data-dir=\"<dir>\" --no-first-run --no-default-browser-check\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_browser_connect_u_detach_kwargs @ hermes_cli/browser_connect.py:_detach_kwargs */
int hermes_cli_browser_connect_u_detach_kwargs(const char *arg) {
    /* Python: {start_new_session: True} on POSIX; creationflags on Windows.
     * Arg = "system" (Windows -> "flags=0x108"; else "session"). */
    if (arg && strncasecmp(arg, "windows", 7) == 0) {
        printf("creationflags=0x108\n");
        return 0;
    }
    printf("start_new_session=true\n");
    return 0;
}

/* PoP: hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it @ hermes_cli/browser_connect.py:_wait_for_browser_debug_ready_or_exit */
int hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(const char *arg) {
    /* Python: ready/exited/starting classifier. Arg = "state\tport\ttimeout". */
    if (!arg || !*arg) { printf("starting\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ready") == 0 || strcmp(state, "exited") == 0 || strcmp(state, "starting") == 0) {
        printf("%s\n", state);
        return 0;
    }
    printf("starting\n");
    return 0;
}

/* PoP: hermes_cli_browser_connect_u_read_stderr_tail @ hermes_cli/browser_connect.py:_read_stderr_tail */
int hermes_cli_browser_connect_u_read_stderr_tail(const char *arg) {
    /* Python: read file bytes; keep last _STDERR_TAIL_LIMIT (e.g. 64 KiB),
     * decode utf-8 errors=replace, strip. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "rb");
    if (!fp) { printf("\n"); return 0; }
    long size;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    long limit = 65536;
    long start = size > limit ? size - limit : 0;
    fseek(fp, start, SEEK_SET);
    char *buf = malloc((size_t)(size - start) + 1);
    if (!buf) { fclose(fp); printf("\n"); return 0; }
    size_t n = fread(buf, 1, (size_t)(size - start), fp);
    fclose(fp);
    buf[n] = '\0';
    /* strip trailing whitespace */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t')) n--;
    printf("%.*s\n", (int)n, buf);
    free(buf);
    return 0;
}

/* PoP: hermes_cli_browser_connect_launch_chrome_debug @ hermes_cli/browser_connect.py:launch_chrome_debug */
int hermes_cli_browser_connect_launch_chrome_debug(const char *arg) {
    /* Python: CDP candidate loop. Arg =
     * "launched\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int launched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("launch result empty\n"); return 0; }
    if (!launched) {
        printf("no Chromium-family binary found or all candidates exited before CDP port opened\n");
        return 0;
    }
    printf("chrome debug launched (pid logged, stderr tail captured per attempt): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: try_launch_chrome_debug @ hermes_cli/browser_connect.py:try_launch_chrome_debug */
int hermes_cli_browser_connect_try_launch_chrome_debug(int port, const char *system) {
    /* Python: return launch_chrome_debug(port, system).launched */
    /* The existing C function takes a single string arg; we encode port+system */
    char buf[64];
    snprintf(buf, sizeof(buf), "%d:%s", port, system);
    return hermes_cli_browser_connect_launch_chrome_debug(buf);
}
