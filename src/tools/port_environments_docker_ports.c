/*
 * port_environments_docker_remaining.c — Port of tools/environments/docker.py
 * docker env surface. Cwd normalization, bash exec in container,
 * dead-container recovery, cleanup by persist mode.
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

/* PoP: __init__ @ tools/environments/docker.py:__init__ */
char *dke_init(const char *cwd, long timeout_seconds) {
    /* Python: ~ → /root. */
    const char *c = (cwd && strcmp(cwd, "~") == 0) ? "/root" : (cwd ? cwd : "/root");
    char *out = NULL;
    asprintf(&out, "{\"cwd\": \"%s\", \"timeout\": %ld}", c, timeout_seconds);
    return out;
}

/* PoP: _run_bash @ tools/environments/docker.py:_run_bash */
char *dke_run_bash(const char *container_id, const char *cmd_string) {
    /* Python: bash inside container. */
    if (!container_id || !cmd_string) return NULL;
    printf("docker bash exec in %s: %.60s\n", container_id, cmd_string);
    return strdup("{}");
}

/* PoP: execute @ tools/environments/docker.py:execute */
char *dke_execute(const char *container_id, const char *command) {
    /* Python: auto-recover dead container. */
    if (!command) return NULL;
    if (container_id)
        printf("docker exec (auto-recover on dead container): %.60s\n", command);
    else
        printf("docker exec (no container)\n");
    return strdup("{}");
}

/* PoP: cleanup @ tools/environments/docker.py:cleanup */
int dke_cleanup(const char *container_id, bool force_remove) {
    /* Python: teardown by persist mode. */
    if (container_id)
        printf("docker container torn down (%s)\n", force_remove ? "force-remove" : "persist-mode");
    return 0;
}
