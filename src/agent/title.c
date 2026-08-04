/*
 * title.c — Session title generation for Hermes C.
 * Simple extractive summarization: uses first N words of first message.
 * Also implements auto_title_session: generate & set title on first exchange.
 */

#include "hermes_agent.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

/* Port of Python: generate_title */
char *generate_title(llm_config_t *cfg, const char *first_message) {
    (void)cfg;

    if (!first_message || !*first_message)
        return strdup("New Session");

    char buf[256];
    size_t pos = 0;
    bool in_code_block = false;

    buf[0] = '\0';

    /* Extract first meaningful sentence — skip code blocks */
    for (const char *p = first_message; *p && pos < sizeof(buf) - 3; p++) {
        /* Track code block boundaries */
        if (*p == '`' && *(p+1) == '`' && *(p+2) == '`') {
            in_code_block = !in_code_block;
            p += 2;
            continue;
        }
        if (in_code_block) continue;

        /* Skip leading whitespace/newlines */
        if (pos == 0 && (*p == ' ' || *p == '\n' || *p == '\r')) continue;

        unsigned char c = (unsigned char)*p;
        if (c == '\n') {
            /* Convert newline to space, stop on double newline */
            if (*(p+1) == '\n') break;
            if (pos > 0 && buf[pos-1] != ' ') buf[pos++] = ' ';
        } else if (c == ' ') {
            /* Collapse consecutive spaces */
            if (pos > 0 && buf[pos-1] != ' ') buf[pos++] = ' ';
        } else if (isprint(c)) {
            buf[pos++] = (char)c;
            /* Stop at sentence-ending punctuation followed by space then EOS/newline */
            if ((c == '.' || c == '!' || c == '?') && pos < sizeof(buf) - 2) {
                const char *next = p + 1;
                while (*next == ' ' || *next == '\n') next++;
                if (*next == '\0' || *next == '\n') break;
            }
        }
    }

    buf[pos] = '\0';

    /* Trim trailing space */
    while (pos > 0 && buf[pos-1] == ' ') buf[--pos] = '\0';

    /* Cap at 80 chars */
    if (pos > 80) {
        buf[80] = '\0';
        pos = 80;
        while (pos > 0 && buf[pos] != ' ') pos--;
        if (pos > 0) buf[pos] = '\0';
    }

    /* Trim trailing period */
    while (pos > 0 && buf[pos-1] == '.') buf[--pos] = '\0';

    if (pos == 0) return strdup("New Session");
    return strdup(buf);
}

/* === Auto-title session (AG20) === */

/*
 * Background thread argument for auto_title_session.
 * Caller must free all fields after the thread finishes.
 */
typedef struct {
    char *session_id;
    char *user_message;
    char *assistant_response;
} auto_title_thread_arg_t;

static void *auto_title_thread_fn(void *arg) {
    auto_title_thread_arg_t *args = (auto_title_thread_arg_t *)arg;
    if (!args) return NULL;

    /* Open session DB — slermes identity: sessions live in
     * $SLERMES_HOME/sessions, never in the Python project's home. */
    const char *home = slermes_home();
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    char db_dir[4096];
    snprintf(db_dir, sizeof(db_dir), "%s/.slermes/sessions", home);
    db_t *db = db_open(db_dir, NULL);

    if (!db) {
        free(args->session_id);
        free(args->user_message);
        free(args->assistant_response);
        free(args);
        return NULL;
    }

    /* Check if title already exists */
    session_meta_t meta;
    db_meta_init(&meta);
    if (db_load_meta(db, args->session_id, &meta) && meta.title[0]) {
        db_close(db);
        free(args->session_id);
        free(args->user_message);
        free(args->assistant_response);
        free(args);
        return NULL;
    }

    /* Generate title from user message */
    char *title = generate_title(NULL, args->user_message);
    if (title && title[0] && strcmp(title, "New Session") != 0) {
        db_meta_init(&meta);
        db_load_meta(db, args->session_id, &meta);
        snprintf(meta.title, sizeof(meta.title), "%s", title);
        meta.updated_at = time(NULL);
        db_save_meta(db, args->session_id, &meta);
    }

    if (title) free(title);
    db_close(db);
    free(args->session_id);
    free(args->user_message);
    free(args->assistant_response);
    free(args);
    return NULL;
}

/*
 * auto_title_session — Generate and set a session title if one doesn't already exist.
 * Mirrors Python's title_generator.py:auto_title_session().
 * Safe to call from any thread.
 */
/* Port of Python: auto_title_session */
/* PoP: auto_title_session @ agent/title_generator.py:_auto_title_session */
void auto_title_session(const char *session_id, const char *user_message,
                        const char *assistant_response) {
    if (!session_id || !*session_id) return;
    if (!user_message) user_message = "";
    if (!assistant_response) assistant_response = "";

    auto_title_thread_arg_t *arg = malloc(sizeof(auto_title_thread_arg_t));
    if (!arg) return;
    arg->session_id = strdup(session_id);
    arg->user_message = strdup(user_message);
    arg->assistant_response = strdup(assistant_response);

    pthread_t tid;
    if (pthread_create(&tid, NULL, auto_title_thread_fn, arg) != 0) {
        free(arg->session_id);
        free(arg->user_message);
        free(arg->assistant_response);
        free(arg);
        return;
    }
    pthread_detach(tid);
}

/*
 * maybe_auto_title — Fire-and-forget title generation after the first exchange.
 * Mirrors Python's title_generator.py:maybe_auto_title().
 * Only generates a title when this appears to be the first 1-2 exchanges.
 * user_message_count: number of user messages so far (including current).
 */
/* Port of Python: maybe_auto_title */
void maybe_auto_title(const char *session_id, const char *user_message,
                      const char *assistant_response,
                      int user_message_count) {
    if (!session_id || !*session_id) return;
    if (!user_message || !*user_message) return;
    if (!assistant_response || !*assistant_response) return;
    /* Only auto-title on first 2 exchanges */
    if (user_message_count > 2) return;

    auto_title_session(session_id, user_message, assistant_response);
}
