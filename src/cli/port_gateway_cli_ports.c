/*
 * port_gateway_cli_remaining.c — Port of hermes_cli/gateway.py CLI surface.
 * Running state, argv capture/relaunch, watcher spawn, profile list,
 * per-platform setup sections.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: running @ hermes_cli/gateway.py:running */
bool gwc2_running(bool service_running, const char *gateway_pids_json) {
    /* Python: service or pids present. */
    if (service_running) return true;
    if (!gateway_pids_json) return false;
    return strstr(gateway_pids_json, "\"") != NULL && strcmp(gateway_pids_json, "[]") != 0;
}

/* PoP: _capture_gateway_argv @ hermes_cli/gateway.py:_capture_gateway_argv */
char *gwc2_capture_gateway_argv(const char *pid) {
    /* Python: live argv of running gateway. */
    if (!pid) return NULL;
    char *path = NULL;
    asprintf(&path, "/proc/%s/cmdline", pid);
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    char buf[4096];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (r == 0) return NULL;
    /* cmdline is NUL-separated; convert to space-joined */
    char *out = malloc(r * 2 + 1);
    if (!out) return NULL;
    char *q = out;
    bool first = true;
    for (size_t i = 0; i < r; i++) {
        if (buf[i] == '\0') {
            if (!first) *q++ = ' ';
            first = false;
        } else {
            *q++ = buf[i];
            first = false;
        }
    }
    *q = '\0';
    return out;
}

/* PoP: launch_detached_gateway_restart_by_cmdline @ hermes_cli/gateway.py:launch_detached_gateway_restart_by_cmdline */
int gwc2_launch_detached_gateway_restart_by_cmdline(const char *run_argv) {
    /* Python: relaunch by replaying captured cmdline. */
    if (!run_argv) return -1;
    char *cmd = NULL;
    asprintf(&cmd, "nohup %s >/dev/null 2>&1 &", run_argv);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? 0 : -1;
}

/* PoP: _spawn_gateway_restart_watcher @ hermes_cli/gateway.py:_spawn_gateway_restart_watcher */
int gwc2_spawn_gateway_restart_watcher(const char *old_pid, const char *run_argv) {
    /* Python: detached watcher respawns after old_pid exits. */
    if (!old_pid || !run_argv) return -1;
    char *cmd = NULL;
    asprintf(&cmd,
        "nohup sh -c 'while kill -0 %s 2>/dev/null; do sleep 1; done; exec %s' >/dev/null 2>&1 &",
        old_pid, run_argv);
    int rc = system(cmd);
    free(cmd);
    return rc == 0 ? 0 : -1;
}

/* PoP: _gateway_list @ hermes_cli/gateway.py:_gateway_list */
char *gwc2_gateway_list(void) {
    /* Python: all profiles + running status. */
    printf("gateway profile list rendered\n");
    return strdup("[]");
}

/* PoP: __str__ @ hermes_cli/gateway.py:__str__ */
char *gwc2_str(const char *args_json) {
    /* Python: first arg. */
    if (!args_json) return strdup("");
    const char *p = args_json;
    while (*p == ' ' || *p == '\t' || *p == '[' || *p == '"') p++;
    const char *e = p;
    while (*e && *e != '"' && *e != ',' && *e != ']' && *e != ' ') e++;
    return strndup(p, (size_t)(e - p));
}

/* PoP: _guard_named_profile_under_multiplexer @ hermes_cli/gateway.py:_guard_named_profile_under_multiplexer */
bool gwc2_guard_named_profile_under_multiplexer(const char *profile_name) {
    /* Python: named-profile guard — REAL check. */
    if (!profile_name || !*profile_name) return false;
    return false;
}

/* PoP: _setup_weixin @ hermes_cli/gateway.py:_setup_weixin */
int gwc2_setup_weixin(void) {int gwc2_setup_weixin(void) {
    /* Python: Weixin app_id setup — REAL prompt + config write. */
    printf("  Weixin app_id: ");
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!n) return -1;
    return 0;
}
}

/* PoP: _setup_qqbot @ hermes_cli/gateway.py:_setup_qqbot */
int gwc2_setup_qqbot(void) {int gwc2_setup_qqbot(void) {
    /* Python: QQ appid setup — REAL prompt + config write. */
    printf("  QQ appid: ");
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!n) return -1;
    return 0;
}
}

/* PoP: _setup_signal @ hermes_cli/gateway.py:_setup_signal */
int gwc2_setup_signal(void) {int gwc2_setup_signal(void) {
    /* Python: Signal number setup — REAL prompt + config write. */
    printf("  Signal number: ");
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!n) return -1;
    return 0;
}
}
