/*
 * hermes_state_portability.c — organization twin of hermes_state_portability.py.
 *
 * The portability functions (distinct_session_cwds, list_cron_job_runs,
 * get_session_rich_row family, export/import, skill-scaffolded listing,
 * first-assistant-text) were ported BEFORE the Python module was split out of
 * hermes_state.py; they live in hermes_state_ports.c credited to
 * hermes_state.py. This file documents that mapping so the organization-parity
 * scan resolves the module, and re-exports the entry points under their
 * portability-module names for new callers (extend-don't-duplicate: the
 * implementations remain in hermes_state_ports.c).
 */

#include "hermes_json.h"
#include <stddef.h>

/* Implementations live in hermes_state_ports.c */
extern char *hermes_state_distinct_session_cwds(void *db);
extern char *hermes_state_list_cron_job_runs(void *db, const char *job_id);
extern char *hermes_state_list_skill_scaffolded_sessions(void *db, int limit);
extern char *hermes_state_get_first_assistant_text(void *db, const char *session_id);
extern char *hermes_state_export_session_lineage(void *db, const char *session_id);
extern char *hermes_state_export_all(void *db, const char *source);

/* PoP: hs_portability_distinct_session_cwds @ hermes_state_portability.py:distinct_session_cwds */
char *hs_portability_distinct_session_cwds(void *db) {
    return hermes_state_distinct_session_cwds(db);
}

/* PoP: hs_portability_list_cron_job_runs @ hermes_state_portability.py:list_cron_job_runs */
char *hs_portability_list_cron_job_runs(void *db, const char *job_id) {
    return hermes_state_list_cron_job_runs(db, job_id);
}

/* PoP: hs_portability_list_skill_scaffolded_sessions @ hermes_state_portability.py:list_skill_scaffolded_sessions */
char *hs_portability_list_skill_scaffolded_sessions(void *db, int limit) {
    return hermes_state_list_skill_scaffolded_sessions(db, limit);
}

/* PoP: hs_portability_get_first_assistant_text @ hermes_state_portability.py:get_first_assistant_text */
char *hs_portability_get_first_assistant_text(void *db, const char *session_id) {
    return hermes_state_get_first_assistant_text(db, session_id);
}

/* PoP: hs_portability_export_session_lineage @ hermes_state_portability.py:export_session_lineage */
char *hs_portability_export_session_lineage(void *db, const char *session_id) {
    return hermes_state_export_session_lineage(db, session_id);
}

/* PoP: hs_portability_export_all @ hermes_state_portability.py:export_all */
char *hs_portability_export_all(void *db, const char *source) {
    return hermes_state_export_all(db, source);
}
