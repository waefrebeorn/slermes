#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/debug_helpers.c — C11 port of tools/debug_helpers.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_debug_helpers.c ── */
/*
 * port_tools_debug_helpers.c — C port of tools/debug_helpers.py
 */


#include "hermes_logger.h"
#include "debug_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: get_debug_session_info @ tools/debug_helpers.py:get_session_info */

/* Port of Python tools/debug_helpers.py:get_session_info */
/* Return a summary dict suitable for returning from get_debug_session_info(). */
char *cli_tools_debug_helpers_get_session_info(const debug_session_t *session)
{
    if (!session) {
        return strdup("{\"enabled\":false,\"session_id\":null,\"log_path\":null,\"total_calls\":0}");
    }

    if (!session->enabled) {
        return strdup("{\"enabled\":false,\"session_id\":null,\"log_path\":null,\"total_calls\":0}");
    }

    /* Build log path */
    char log_path[1024];
    if (session->log_dir[0]) {
        snprintf(log_path, sizeof(log_path),
            "%s/%s_debug_%s.json",
            session->log_dir, session->tool_name, session->session_id);
    } else {
        snprintf(log_path, sizeof(log_path),
            "%s_debug_%s.json",
            session->tool_name, session->session_id);
    }

    /* Build JSON */
    size_t buf_size = 512 + strlen(log_path) + strlen(session->session_id);
    char *json = (char *)malloc(buf_size);
    if (!json) return NULL;

    snprintf(json, buf_size,
        "{"
        "\"enabled\":true,"
        "\"session_id\":\"%s\","
        "\"log_path\":\"%s\","
        "\"total_calls\":%d"
        "}",
        session->session_id, log_path, session->call_count);

    return json;
}


/* ── from src/tools/port_debug_helpers_ports.c ── */
/*
 * port_debug_helpers_remaining.c — Port of tools/debug_helpers.py tool-log
 * surface. Env-gated enable, active probe, real JSON flush.
 */
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

/* Module-level enabled state, mirroring Python DebugToolLog.enabled. */
static bool g_dbh_enabled = false;

/* PoP: dbh_init @ tools/debug_helpers.py:__init__ */
char *dbh_init(const char *tool_name, const char *env_var) {
    /* Python: env-gated tool logging. */
    const char *v = env_var ? getenv(env_var) : NULL;
    bool enabled = v && (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
    g_dbh_enabled = enabled;
    char *out = NULL;
    asprintf(&out, "{\"tool_name\": \"%s\", \"enabled\": %s}",
             tool_name ? tool_name : "", enabled ? "true" : "false");
    return out;
}

/* PoP: dbh_active @ tools/debug_helpers.py:active */
bool dbh_active(void) {
    /* Python: DebugToolLog.active == self.enabled (env-gated at init). */
    return g_dbh_enabled;
}

/* PoP: dbh_save @ tools/debug_helpers.py:save */
int dbh_save(const char *log_dir, const char *tool_name, const char *log_json) {
    /* Python: flush to JSON file — REAL write. */
    if (!log_dir || !tool_name) return -1;
    char *path = NULL;
    asprintf(&path, "%s/%s_debug.json", log_dir, tool_name);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fprintf(w, "%s\n", log_json ? log_json : "{}");
    fclose(w);
    free(path);
    return 0;
}

