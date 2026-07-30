/*
 * app_session_entry.h — App-state session list entry.
 *
 * Self-contained type for the desktop/app-state subsystem's in-memory session
 * list. Deliberately named `app_session_entry_t` (NOT `session_entry_t`) to
 * avoid colliding with libdb's canonical `session_entry_t` (id + session_meta_t).
 * The two structs serve different subsystems and must not share a name.
 *
 * C11 only. No external includes beyond stddef/stdbool.
 */
#ifndef SLERMES_APP_SESSION_ENTRY_H
#define SLERMES_APP_SESSION_ENTRY_H

#include <stddef.h>
#include <stdbool.h>

typedef struct app_session_entry {
    char   id[64];
    char   title[256];
    char   source[32];
    char   model[128];
    int    msg_count;
    int    tokens;
    long   started_at;
} app_session_entry_t;

#endif /* SLERMES_APP_SESSION_ENTRY_H */
