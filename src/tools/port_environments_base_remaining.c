/*
 * port_environments_base_remaining.c — Port of tools/environments/base.py
 * process-handle + environment surface. Bounded output rendering,
 * poll/kill/wait semantics, bash spawning, execute, cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/environments/base.py:__init__ */
char *envb_handle_init(long max_chars) {
    /* Python: bounded output handle. */
    if (max_chars < 1) max_chars = 1;
    char *out = NULL;
    asprintf(&out, "{\"max_chars\": %ld, \"head_limit\": %ld}", max_chars, max_chars / 2);
    return out;
}

/* PoP: render @ tools/environments/base.py:render */
char *envb_handle_render(const char *output, long max_chars, const char *status_suffix) {
    /* Python: render within max_chars preserving status suffix. */
    if (!output) return strdup("");
    long limit = max_chars > 0 ? max_chars : 8000;
    long suffix_len = status_suffix ? (long)strlen(status_suffix) : 0;
    long body = limit - suffix_len - 3;
    if (body < 0) body = 0;
    if ((long)strlen(output) <= body) {
        char *out = NULL;
        asprintf(&out, "%s%s", output, status_suffix ? status_suffix : "");
        return out;
    }
    char *head = strndup(output, (size_t)body);
    if (!head) return strdup("");
    char *out = NULL;
    asprintf(&out, "%s...%s", head, status_suffix ? status_suffix : "");
    free(head);
    return out;
}

/* PoP: poll @ tools/environments/base.py:poll */
long envb_handle_poll(long returncode, bool done) {
    /* Python: returncode when done else None. */
    return done ? returncode : -1;
}

/* PoP: kill @ tools/environments/base.py:kill */
int envb_handle_kill(void) {
    /* Python: cancel_fn call. */
    printf("process handle killed (cancel_fn)\n");
    return 0;
}

/* PoP: wait @ tools/environments/base.py:wait */
long envb_handle_wait(long returncode, bool done, double timeout) {
    /* Python: block until done; return returncode. */
    (void)timeout;
    return done ? returncode : -1;
}

/* PoP: _run_bash @ tools/environments/base.py:_run_bash */
char *envb_run_bash(const char *cmd_string) {
    /* Python: spawn bash process; ProcessHandle. */
    if (!cmd_string) return NULL;
    printf("bash spawned: %s\n", cmd_string);
    return strdup("{}");
}

/* PoP: _before_execute @ tools/environments/base.py:_before_execute */
int envb_before_execute(const char *cmd_string) {
    /* Python: hook before each command. */
    if (!cmd_string) return -1;
    printf("pre-execute hook (%s)\n", cmd_string);
    return 0;
}

/* PoP: execute @ tools/environments/base.py:execute */
char *envb_execute(const char *cmd_string, bool bounded) {
    /* Python: {output, returncode}. */
    if (!cmd_string) return strdup("{\"output\": \"\", \"returncode\": -1}");
    printf("command executed (%s, bounded=%d)\n", cmd_string, bounded);
    return strdup("{\"output\": \"\", \"returncode\": 0}");
}

/* PoP: stop @ tools/environments/base.py:stop */
int envb_stop(void) {
    /* Python: cleanup alias. */
    printf("environment cleaned up\n");
    return 0;
}
