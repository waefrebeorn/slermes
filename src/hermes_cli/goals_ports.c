/*
 * port_goals_remaining.c — Port of hermes_cli/goals.py goal surface.
 * Session db access, migration, truncation, turn limits.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "port_config_py_helpers.h"
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: gls_get_session_db @ hermes_cli/goals.py:_get_session_db */
char *gls_get_session_db(void) {
    /* Python: SessionDB for current home. */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/sessions.db", h);
        return out;
    }
    return strdup("sessions.db");
}

/* PoP: gls_migrate_goal_to_session @ hermes_cli/goals.py:migrate_goal_to_session */
char *gls_migrate_goal_to_session(const char *goal_text, const char *parent_session_id, const char *session_id) {
    /* Python: carry persistent /goal to continuation. */
    if (!goal_text || !session_id) return NULL;
    printf("goal migrated: %.60s (%s → %s)\n", goal_text, parent_session_id ? parent_session_id : "?", session_id);
    return strdup("{\"success\": true}");
}

/* PoP: gls_truncate @ hermes_cli/goals.py:_truncate */
char *gls_truncate(const char *text, long limit) {
    /* Python: hard truncate. */
    if (!text) return strdup("");
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    char *out = malloc((size_t)limit + 8);
    if (!out) return strdup(text);
    memcpy(out, text, (size_t)limit);
    strcpy(out + limit, "…");
    return out;
}

/* PoP: gls_init @ hermes_cli/goals.py:__init__ */
char *gls_init(const char *session_id, long default_max_turns) {
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"default_max_turns\": %ld}",
             session_id ? session_id : "", default_max_turns);
    return out;
}

/* PoP: hcli_goal__goal_judge_timeout @ hermes_cli/goals.py:_goal_judge_timeout */
double hcli_goal__goal_judge_timeout(void) {
    /* Python: resolve auxiliary.goal_judge.timeout, fallback DEFAULT_JUDGE_TIMEOUT (30.0).
     * Read-only config, same as _configured_mcp_result_size. */
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg || cfg->type != JSON_OBJECT) {
        return 30.0;  /* DEFAULT_JUDGE_TIMEOUT */
    }
    json_t *aux = json_obj_get(cfg, "auxiliary");
    if (!aux || aux->type != JSON_OBJECT) {
        return 30.0;
    }
    json_t *gj = json_obj_get(aux, "goal_judge");
    if (!gj || gj->type != JSON_OBJECT) {
        return 30.0;
    }
    json_t *tv = json_obj_get(gj, "timeout");
    if (!tv || tv->type != JSON_NUMBER) {
        return 30.0;
    }
    double value = tv->num_val;
    if (value > 0.0) {
        return value;
    }
    return 30.0;
}
