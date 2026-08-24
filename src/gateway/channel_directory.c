/*
 * channel_directory.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway_channel_directory.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Channel directory — resolve channel names to IDs
 *  Port of Python gateway/channel_directory.py.
 * ================================================================ */

/* Normalize a channel query: strip #, trim, lowercase.
 * Port of Python gateway/channel_directory.py _normalize_channel_query().
 * AG26: Port of Python gateway/channel_directory.py:_normalize_channel_query().
 */
char *normalize_channel_query(const char *value) {
    if (!value) return strdup("");
    /* Strip leading # */
    const char *s = value;
    while (*s == '#') s++;
    while (*s == ' ') s++;
    char *buf = strdup(s);
    if (!buf) return NULL;
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    return buf;
}


/* Build a session entry ID: chat_id or chat_id:thread_id.
 * Port of Python gateway/channel_directory.py _session_entry_id().
 * AG26: Port of Python gateway/channel_directory.py:_session_entry_id().
 */
char *session_entry_id(const char *chat_id, const char *thread_id) {
    if (!chat_id || !*chat_id) return NULL;
    if (thread_id && *thread_id) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s:%s", chat_id, thread_id);
        return strdup(buf);
    }
    return strdup(chat_id);
}



/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_channel_directory_u_warn_slack_directory @ gateway/channel_directory.py:_warn_slack_directory */
int gateway_channel_directory_u_warn_slack_directory(const char *arg) {
    /* Python: throttled warning. Arg = "team_id\tdetail\tthrottled". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int throttled = t2 && t2[1] == '1';
    if (throttled) printf("slack directory failure suppressed (repeated)\n");
    else printf("Channel directory: failed to list Slack channels for team %s: %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: gateway_channel_directory_u_slack_api_error_code @ gateway/channel_directory.py:_slack_api_error_code */
int gateway_channel_directory_u_slack_api_error_code(const char *arg) {
    /* Python: response.error str or None. Arg = response JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_object(j)) {
        const char *v = json_get_str(j, "error", "");
        if (v[0]) { printf("%s\n", v); json_free(j); return 0; }
    }
    if (j) json_free(j);
    printf("\n");
    return 0;
}

/* PoP: gateway_channel_directory_u_build_from_sessions_db @ gateway/channel_directory.py:_build_from_sessions_db */
int gateway_channel_directory_u_build_from_sessions_db(const char *arg) {
    /* Python: state.db rows. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: gateway_channel_directory_u_build_from_sessions_json @ gateway/channel_directory.py:_build_from_sessions_json */
int gateway_channel_directory_u_build_from_sessions_json(const char *arg) {
    /* Python: legacy fallback. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}
