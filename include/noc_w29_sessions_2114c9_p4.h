/*
 * noc_w29_sessions_2114c9_p4.h - C surface for upstream
 * persistence/sqlite/conversation/sessions.py (gate w29_sessions_2114c9_p4).
 *
 * Gate p4: the listing/recency/roster envelopes a row is wrapped in - the
 * SessionListEntry of _list_entry, the recency entry, the Bots roster session
 * and worker shapes, the preview derivation of the roster summary, the compact
 * row projections of list_row / live_listing_facts / list_recent_sessions and
 * the pending-admission grouping.
 */
#ifndef NOC_W29_SESSIONS_2114C9_P4_H
#define NOC_W29_SESSIONS_2114C9_P4_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {row, compact} -> SessionListEntry  (sessions.py:_list_entry) */
json_t *w29_sessions_2114c9_p4_list_entry(const json_t *req);
/* req {row} -> SessionRecencyEntry  (sessions.py:_recency_entry) */
json_t *w29_sessions_2114c9_p4_recency_entry(const json_t *req);
/* req {fields} -> RosterSession or null  (sessions.py:_roster_session) */
json_t *w29_sessions_2114c9_p4_roster_session(const json_t *req);
/* req {root, target, tip, text} -> the roster summary fields  (sessions.py:_read_bot_sessions.summary) */
json_t *w29_sessions_2114c9_p4_roster_summary(const json_t *req);
/* req {canonical, latest} -> fields['last_session']  (sessions.py:_read_bot_sessions) */
json_t *w29_sessions_2114c9_p4_roster_last_session(const json_t *req);
/* req {row} -> SessionListRow  (sessions.py:SQLiteSessionRepository.list_row) */
json_t *w29_sessions_2114c9_p4_list_row(const json_t *req);
/* req {row} -> LiveListingRow  (sessions.py:SQLiteSessionRepository.live_listing_facts) */
json_t *w29_sessions_2114c9_p4_live_listing_row(const json_t *req);
/* req {rows} -> {target_session_id: [PendingAdmissionRef, ...]}  (sessions.py:live_listing_facts) */
json_t *w29_sessions_2114c9_p4_pending_by_session(const json_t *req);
/* req {row} -> RecentSessionEntry  (sessions.py:SQLiteSessionRepository.list_recent_sessions) */
json_t *w29_sessions_2114c9_p4_recent_entry(const json_t *req);
/* req {row} -> RosterWorker  (sessions.py:SQLiteSessionRepository.roster_sessions) */
json_t *w29_sessions_2114c9_p4_worker_session(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
