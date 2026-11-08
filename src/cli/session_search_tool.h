/*
 * session_search_tool.h — session-search-tool message type + helpers.
 *
 * Module-internal header (port_tools_session_search_tool.c <->
 * session_search_tool.c).
 */
#ifndef SESSION_SEARCH_TOOL_H
#define SESSION_SEARCH_TOOL_H

#include <stddef.h>
#include "sqlite3.h"
#include "libjson/json.h"

#define SST_MAX_QUERY_LEN 1024

typedef struct {
    long   id;
    char   role[32];
    char   content[16384];
    char   tool_name[256];
    char   tool_calls[8192];
    char   tool_call_id[256];
    double timestamp;
    int    has_tool_name;
    int    has_tool_calls;
    int    has_tool_call_id;
} sst_msg_t;

/* Decode content (strip the JSON-encoding prefix the Python DB applies). */
json_t *sst_shape_message_obj(const sst_msg_t *m, long anchor_id);
void sst_decode_content(const char *raw, char *out, size_t outsz);
int  sst_fetch_message(sqlite3 *db, long msg_id, sst_msg_t *m);
int  sst_resolve_to_parent_db(sqlite3 *db, const char *session_id,
                              char *root_out, size_t root_size);
void sst_sanitize_fts5(const char *in, char *out, size_t outsz);
int  sst_anchored_view(sqlite3 *db, const char *session_id, long anchor_id,
                       int window, int bookend,
                       json_t *window_arr, json_t *bookend_start_arr,
                       json_t *bookend_end_arr, int *before_out, int *after_out);

#endif /* SESSION_SEARCH_TOOL_H */
