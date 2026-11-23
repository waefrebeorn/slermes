/*
 * session_command_sink.c — C home for upstream Hermes gateway/session_command_sink.py.
 *
 * PoP: cmdsink_capture @ gateway/session_command_sink.py:capture
 * PoP: cmdsink_active @ gateway/session_command_sink.py:active_sink
 * PoP: cmdsink_sink_t @ gateway/session_command_sink.py:CommandSink
 *
 * capture() is a @contextmanager that sets a ContextVar to a fresh CommandSink
 * (notices=[], prompt=None, display=None), yields it, then resets the token on
 * exit.  A ContextVar is a per-context stack of set/reset values; this static
 * LIFO stack models that, so nested captures and restores behave identically.
 */
#include "noc_cmdsink.h"

#include <stdlib.h>
#include <string.h>

#define CMD_SINK_MAX 32

typedef struct {
    char   **notices;
    size_t   n_notices;
    size_t   cap;
    char    *prompt;
    char    *display;
} sink_entry_t;

static sink_entry_t stack[CMD_SINK_MAX];
static int stack_depth = 0;

static void sink_reset(sink_entry_t *e) {
    e->notices = NULL; e->n_notices = 0; e->cap = 0;
    e->prompt = NULL; e->display = NULL;
}

static char *dup_or_null(const char *s) {
    return s ? strdup(s) : NULL;
}

int cmdsink_capture(void) {
    if (stack_depth >= CMD_SINK_MAX) return stack_depth;
    sink_reset(&stack[stack_depth]);
    stack_depth++;
    return stack_depth;
}

int cmdsink_release(void) {
    if (stack_depth <= 0) return 0;
    sink_entry_t *e = &stack[stack_depth - 1];
    for (size_t i = 0; i < e->n_notices; ++i) free(e->notices[i]);
    free(e->notices);
    free(e->prompt);
    free(e->display);
    sink_reset(e);
    stack_depth--;
    return stack_depth;
}

int cmdsink_depth(void) {
    return stack_depth;
}

int cmdsink_active(cmdsink_sink_t *out) {
    if (stack_depth <= 0 || !out) return 0;
    sink_entry_t *e = &stack[stack_depth - 1];
    out->notices = e->n_notices;
    out->prompt  = e->prompt;
    out->display = e->display;
    return 1;
}

const char *cmdsink_notice_at(size_t index) {
    if (stack_depth <= 0) return NULL;
    sink_entry_t *e = &stack[stack_depth - 1];
    return index < e->n_notices ? e->notices[index] : NULL;
}

void cmdsink_add_notice(const char *text) {
    if (stack_depth <= 0) return;
    sink_entry_t *e = &stack[stack_depth - 1];
    if (e->n_notices == e->cap) {
        size_t nc = e->cap ? e->cap * 2 : 4;
        char **nn = (char **)realloc(e->notices, nc * sizeof(char *));
        if (!nn) return;
        e->notices = nn; e->cap = nc;
    }
    e->notices[e->n_notices++] = strdup(text ? text : "");
}

void cmdsink_set_prompt(const char *value) {
    if (stack_depth <= 0) return;
    sink_entry_t *e = &stack[stack_depth - 1];
    free(e->prompt);
    e->prompt = dup_or_null(value);
}

void cmdsink_set_display(const char *value) {
    if (stack_depth <= 0) return;
    sink_entry_t *e = &stack[stack_depth - 1];
    free(e->display);
    e->display = dup_or_null(value);
}
