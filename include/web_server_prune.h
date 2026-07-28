/*
 * web_server_prune.h — session prune engine (faithful C11 port of
 * SessionDB._prune_filter_where / list_prune_candidates / prune_sessions
 * from hermes_state.py and the _prune_sessions endpoint wrapper from
 * hermes_cli/web_server.py).
 *
 * The endpoint wrapper takes the request body as a json object; a key
 * counts as "explicitly set" when present in the body (mirrors pydantic's
 * model_fields_set for a JSON request).
 */
#ifndef WEB_SERVER_PRUNE_H
#define WEB_SERVER_PRUNE_H

#include <stdbool.h>

#include "libjson/json.h"

/* Filter set for _prune_filter_where. has_* flags gate numeric/tri-state
 * fields (Python None vs value); strings are NULL = absent. Python
 * truthiness note: source/title_like/etc. use `if value:` so empty strings
 * are treated as absent — pass NULL for "". */
typedef struct {
    bool has_last_active_before;  double last_active_before;
    bool has_last_active_after;   double last_active_after;
    bool has_started_before;      double started_before;
    bool has_started_after;       double started_after;
    const char *source;
    const char *title_like;
    const char *end_reason;
    const char *cwd_prefix;
    bool has_min_messages;        int min_messages;
    bool has_max_messages;        int max_messages;
    int archived;                 /* -1 = None, 0 = False, 1 = True */
    const char *model_like;
    const char *provider;
    const char *user_id;
    const char *chat_id;
    const char *chat_type;
    const char *branch_like;
    bool has_min_tokens;          long long min_tokens;
    bool has_max_tokens;          long long max_tokens;
    bool has_min_cost;            double min_cost;
    bool has_max_cost;            double max_cost;
    bool has_min_tool_calls;      int min_tool_calls;
    bool has_max_tool_calls;      int max_tool_calls;
} ws_prune_filters_t;

/* list_prune_candidates: rows oldest-first with id/source/title/model/
 * started_at/last_active/ended_at/message_count/archived.
 * older_than_days: pass has_older=false for None. */
json_t *ws_prune_candidates(const char *db_path, bool has_older,
                            double older_than_days,
                            const ws_prune_filters_t *f);

/* prune_sessions (no sessions_dir sweep — web endpoint parity is handled
 * by the caller). Returns count deleted, -1 on open failure.
 * NOTE: Python's default is older_than_days=90; callers replicate that by
 * passing has_older=true, older_than_days=90. */
int ws_prune_sessions(const char *db_path, bool has_older,
                      double older_than_days, const ws_prune_filters_t *f);

/* _prune_sessions endpoint wrapper (web_server.py): body is the parsed
 * request JSON. Applies the 400 guard (older_than_days < 1 without a
 * window), the implicit-cutoff suppression rules, dry-run projection
 * shape, and the archived tri-state from include_archived. Returns the
 * HTTP response object; on 400 returns {"status":400,"detail":...}. */
json_t *ws_prune_endpoint(const char *db_path, const json_t *body);

#endif /* WEB_SERVER_PRUNE_H */
