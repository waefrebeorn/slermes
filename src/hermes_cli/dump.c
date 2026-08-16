/**
 * port_dump.c — Port of Python: cli.py (dump helpers)
 *
 * Real C implementations for dump/git functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

/* Port of Python: get_git_commit_date */
char *get_git_commit_date(json_t *project_root)
{
    if (!project_root) {
        hermes_log(LOG_WARNING, "port", "get_git_commit_date: null project_root");
        return strdup("unknown");
    }
    const char *root = json_node_get_string(project_root);
    if (!root) return strdup("unknown");

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git -C '%s' log -1 --format=%%ci 2>/dev/null", root);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        hermes_log(LOG_WARNING, "port", "get_git_commit_date: git command failed");
        return strdup("unknown");
    }
    char buf[256];
    if (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = '\0';
        pclose(fp);
        hermes_log(LOG_DEBUG, "port", "get_git_commit_date: %s -> %s", root, buf);
        return strdup(buf);
    }
    pclose(fp);
    return strdup("unknown");
}

/* Port of Python: _get_git_commit_date */
char *_get_git_commit_date(void *ctx, void *project_root)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_get_git_commit_date: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "_get_git_commit_date: called");
    if (project_root) {
        hermes_log(LOG_DEBUG, "port", "_get_git_commit_date: project_root is set");
    }
    return get_git_commit_date((json_t *)project_root);
}
