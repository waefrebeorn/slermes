/*
 * channel_directory.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
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

