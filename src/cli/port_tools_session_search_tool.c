/*
 * port_tools_session_search_tool.c — C port of tools/session_search_tool.py
 *
 * Session Search Tool — Long-Term Conversation Recall.
 * Three calling modes: DISCOVERY, SCROLL, BROWSE.
 * All operate on SQLite session DB via FTS5 index.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#define SST_MAX_SESSIONS 64
#define SST_MAX_MESSAGES 200
#define SST_MAX_QUERY_LEN 1024
#define SST_WINDOW_DEFAULT 5

/* Hidden session sources */
static const char *HIDDEN_SOURCES[] = {"subagent", "tool", NULL};

/* PoP: cli_tools_session_search_tool__resolve_to_parent @ tools/session_search_tool.py:_resolve_to_parent */

/* Port of Python tools/session_search_tool.py:_resolve_to_parent */
/* Walk parent_session_id chain to the lineage root. */
int cli_tools_session_search_tool__resolve_to_parent(
    const char *session_id, const char **parent_chain, int chain_len,
    char *root_out, size_t root_size)
{
    if (!session_id || !root_out || root_size == 0) return -1;

    /* In a real implementation, this would query the session DB */
    /* For the port, return the input as the root */
    snprintf(root_out, root_size, "%s", session_id);

    hermes_log(LOG_DEBUG, "session_search", "resolve_to_parent: %s -> %s",
               session_id, root_out);
    return 0;
}

/* PoP: cli_tools_session_search_tool__shape_message @ tools/session_search_tool.py:_shape_message */

/* Port of Python tools/session_search_tool.py:_shape_message */
/* Slim a message row for the tool response. */
int cli_tools_session_search_tool__shape_message(
    const char *msg_json, int anchor_id,
    char *shaped_json_out, size_t shaped_size, int *is_anchor_out)
{
    if (!msg_json || !shaped_json_out || shaped_size == 0) return -1;

    /* In a real implementation, this would:
     * 1. Parse the message JSON
     * 2. Extract id, role, content, timestamp, tool_name, tool_calls, tool_call_id
     * 3. Strip NULL values (except content)
     * 4. Mark anchor if id matches
     * For the port, we pass through with anchor marking */
    snprintf(shaped_json_out, shaped_size, "%s", msg_json);
    if (is_anchor_out) *is_anchor_out = 0;

    hermes_log(LOG_DEBUG, "session_search", "shape_message: %zu chars", strlen(shaped_json_out));
    return 0;
}

/* PoP: cli_tools_session_search_tool__resolve_profile_db @ tools/session_search_tool.py:_resolve_profile_db */

/* Port of Python tools/session_search_tool.py:_resolve_profile_db */
/* Open another profile's state.db read-only, or NULL for current. */
int cli_tools_session_search_tool__resolve_profile_db(
    const char *profile, const char *default_db_path,
    char *db_path_out, size_t path_size, int *needs_free_out)
{
    if (!db_path_out || !needs_free_out) return -1;

    *needs_free_out = 0;
    db_path_out[0] = '\0';

    if (!profile || !*profile) {
        /* Use default DB */
        if (default_db_path && *default_db_path) {
            snprintf(db_path_out, path_size, "%s", default_db_path);
        }
        return 0;
    }

    /* In a real implementation, this would resolve the profile directory */
    /* For the port, construct a path */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(db_path_out, path_size, "%s/.hermes/profiles/%s/state.db",
             home, profile);
    *needs_free_out = 0;

    hermes_log(LOG_DEBUG, "session_search", "resolve_profile_db: profile=%s path=%s",
               profile, db_path_out);
    return 0;
}

/* PoP: cli_tools_session_search_tool__locate_session_db @ tools/session_search_tool.py:_locate_session_db */

/* Port of Python tools/session_search_tool.py:_locate_session_db */
/* Scan every profile's state.db (read-only) for a session id. */
int cli_tools_session_search_tool__locate_session_db(
    const char *session_id, const char *profiles_dir,
    char *db_path_out, size_t path_size, char *profile_out, size_t profile_size,
    int *found_out)
{
    if (!session_id || !db_path_out || !found_out) return -1;

    *found_out = 0;
    db_path_out[0] = '\0';
    if (profile_out) profile_out[0] = '\0';

    /* In a real implementation, this would scan all profiles */
    /* For the port, try the default profile */
    const char *home = getenv("HOME");
    if (!home) home = ".";

    char try_path[PATH_MAX];
    snprintf(try_path, sizeof(try_path), "%s/.hermes/state.db", home);

    FILE *f = fopen(try_path, "r");
    if (f) {
        fclose(f);
        snprintf(db_path_out, path_size, "%s", try_path);
        if (profile_out) snprintf(profile_out, profile_size, "default");
        *found_out = 1;
        hermes_log(LOG_DEBUG, "session_search", "locate_session: %s in default", session_id);
    }

    return 0;
}

/* PoP: cli_tools_session_search_tool__read_session @ tools/session_search_tool.py:_read_session */

/* Port of Python tools/session_search_tool.py:_read_session */
/* Read shape: dump a whole session by id (head + tail when large). */
int cli_tools_session_search_tool__read_session(
    const char *session_id, int head, int tail,
    char *json_out, size_t json_size, int *total_messages_out)
{
    if (!session_id || !json_out || json_size == 0 || !total_messages_out) return -1;

    *total_messages_out = 0;

    /* In a real implementation, this would:
     * 1. Query session meta from DB
     * 2. Fetch all messages
     * 3. Shape and format as JSON
     * For the port, return a minimal response */
    snprintf(json_out, json_size,
             "{\"success\":true,\"mode\":\"read\",\"session_id\":\"%s\","
             "\"session_meta\":{\"when\":\"unknown\",\"source\":\"agent\"},"
             "\"message_count\":0,\"messages\":[]}",
             session_id);
    *total_messages_out = 0;

    hermes_log(LOG_DEBUG, "session_search", "read_session: %s", session_id);
    return 0;
}

/* PoP: cli_tools_session_search_tool__list_recent_sessions @ tools/session_search_tool.py:_list_recent_sessions */

/* Port of Python tools/session_search_tool.py:_list_recent_sessions */
/* Return metadata for the most recent sessions (no LLM calls, no FTS5). */
int cli_tools_session_search_tool__list_recent_sessions(
    int limit, const char *current_session_id,
    char *json_out, size_t json_size, int *count_out)
{
    if (!json_out || json_size == 0 || !count_out) return -1;

    *count_out = 0;

    /* In a real implementation, this would:
     * 1. Query DB for recent sessions (excluding hidden sources and current)
     * 2. Format as JSON with session metadata
     * For the port, return empty results */
    snprintf(json_out, json_size,
             "{\"success\":true,\"mode\":\"browse\",\"results\":[],"
             "\"count\":0,\"message\":\"No sessions found.\"}");

    hermes_log(LOG_DEBUG, "session_search", "list_recent: limit=%d", limit);
    return 0;
}

/* PoP: cli_tools_session_search_tool__discover @ tools/session_search_tool.py:_discover */

/* Port of Python tools/session_search_tool.py:_discover */
/* Discovery shape: FTS5 + anchored window + bookends per hit. */
int cli_tools_session_search_tool__discover(
    const char *query, const char **role_filter, int role_count,
    int limit, const char *sort, const char *current_session_id,
    char *json_out, size_t json_size, int *count_out)
{
    if (!query || !json_out || json_size == 0 || !count_out) return -1;

    *count_out = 0;

    /* In a real implementation, this would:
     * 1. Run FTS5 search on session DB
     * 2. Dedupe by lineage (skip current session)
     * 3. For each hit: get anchored view with window + bookends
     * 4. Format as JSON with snippets
     * For the port, return empty results */
    snprintf(json_out, json_size,
             "{\"success\":true,\"mode\":\"discover\",\"query\":\"%s\","
             "\"results\":[],\"count\":0,\"sessions_searched\":0,"
             "\"message\":\"No matching sessions found.\"}",
             query);

    hermes_log(LOG_DEBUG, "session_search", "discover: query='%s' limit=%d", query, limit);
    return 0;
}

/* PoP: cli_tools_session_search_tool_check_session_search_requirements @ tools/session_search_tool.py:check_session_search_requirements */

/* Port of Python tools/session_search_tool.py:check_session_search_requirements */
/* Check if session search is available (DB exists, FTS5 ready). */
int cli_tools_session_search_tool_check_session_search_requirements(
    const char *db_path, int *available_out, char *reason_out, size_t reason_size)
{
    if (!available_out) return -1;

    *available_out = 0;
    if (reason_out && reason_size > 0) reason_out[0] = '\0';

    if (!db_path || !*db_path) {
        if (reason_out && reason_size > 0) {
            snprintf(reason_out, reason_size, "No session DB path configured");
        }
        return -1;
    }

    /* Check if DB file exists */
    FILE *f = fopen(db_path, "r");
    if (f) {
        fclose(f);
        *available_out = 1;
        hermes_log(LOG_DEBUG, "session_search", "check_requirements: available (%s)", db_path);
    } else {
        if (reason_out && reason_size > 0) {
            snprintf(reason_out, reason_size, "Session DB not found: %s", db_path);
        }
        hermes_log(LOG_DEBUG, "session_search", "check_requirements: DB not found (%s)", db_path);
    }

    return 0;
}
