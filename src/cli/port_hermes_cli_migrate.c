/*
 * port_hermes_cli_migrate.c — C port of hermes_cli/migrate.py (select helpers)
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Resolve the active HERMES_HOME (mirrors hermes_cli.config.get_hermes_home):
 * $HERMES_HOME if set, else ~/.hermes. Returns malloc'd string. */
static char *migrate_hermes_home(void) {
    const char *env = getenv("HERMES_HOME");
    if (env && env[0]) return strdup(env);
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    char *out = malloc(strlen(home) + 16);
    snprintf(out, strlen(home) + 16, "%s/.hermes", home);
    return out;
}

/* PoP: cli_hermes_cli_migrate__resolve_config_path @ hermes_cli/migrate.py:_resolve_config_path */
/* Best-effort: locate the active config.yaml on disk — <hermes_home>/config.yaml.
 * Returns a malloc'd path string (caller frees) or NULL on alloc failure. */
char *cli_hermes_cli_migrate__resolve_config_path(void) {
    char *home = migrate_hermes_home();
    if (!home) return NULL;
    size_t need = strlen(home) + strlen("/config.yaml") + 1;
    char *path = malloc(need);
    if (!path) { free(home); return NULL; }
    snprintf(path, need, "%s/config.yaml", home);
    free(home);
    return path;
}
