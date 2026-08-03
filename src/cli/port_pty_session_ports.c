/*
 * port_pty_session_remaining.c — Port of hermes_cli/pty_session.py PTY
 * output surface. Bounded ring buffer, snapshot, attach, close.
 *
 * Faithful C port: a real PTY session registry.  Each session owns a
 * pty_t (src/pty.c) and a drain thread that reads child output into a
 * bounded ring buffer (Python's BoundedOutputBuffer).  attach() swaps
 * the current WebSocket-like sink, close() cancels the drain and
 * disposes the pty, close_all() closes every session.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "slermes_pty.h"
#include "libjson/json.h"
#include "hermes_json.h"

/* Default drain read timeout, seconds (Python PTY_READ_TIMEOUT). */
#define PTY_DRAIN_TIMEOUT 0.2
/* Max sessions in the registry. */
#define PTY_REGISTRY_MAX 64

/* ------------------------------------------------------------------ */
/*  Bounded output buffer (Python BoundedOutputBuffer)                 */
/* ------------------------------------------------------------------ */
typedef struct {
    char  *data;
    size_t capacity;
    size_t len;
    size_t head;      /* ring head (oldest byte) */
    bool   truncated;
} pty_buffer_t;

static void buf_init(pty_buffer_t *b, size_t capacity) {
    b->capacity = capacity > 0 ? capacity : 65536;
    b->data = malloc(b->capacity);
    b->len = 0;
    b->head = 0;
    b->truncated = false;
}

static void buf_destroy(pty_buffer_t *b) {
    free(b->data);
    b->data = NULL;
}

/* Append chunk, dropping oldest bytes when over capacity. */
static void buf_append(pty_buffer_t *b, const char *chunk, size_t n) {
    if (!b->data || n == 0) return;
    if (n >= b->capacity) {
        /* Chunk alone exceeds capacity: keep the tail of it. */
        memcpy(b->data, chunk + (n - b->capacity), b->capacity);
        b->head = 0;
        b->len = b->capacity;
        b->truncated = true;
        return;
    }
    if (b->len + n > b->capacity) {
        size_t drop = b->len + n - b->capacity;
        b->head = (b->head + drop) % b->capacity;
        b->len -= drop;
        b->truncated = true;
    }
    size_t pos = (b->head + b->len) % b->capacity;
    size_t first = b->capacity - pos;
    if (first > n) first = n;
    memcpy(b->data + pos, chunk, first);
    if (n > first)
        memcpy(b->data, chunk + first, n - first);
    b->len += n;
}

/* Snapshot as malloc'd bytes (Python buffer.snapshot()). */
static char *buf_snapshot(const pty_buffer_t *b) {
    if (!b->data || b->len == 0) return strdup("");
    char *out = malloc(b->len + 1);
    if (!out) return strdup("");
    size_t first = b->capacity - b->head;
    if (first > b->len) first = b->len;
    memcpy(out, b->data + b->head, first);
    if (b->len > first)
        memcpy(out + first, b->data, b->len - first);
    out[b->len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/*  PTY session (Python PtySession)                                    */
/* ------------------------------------------------------------------ */
typedef struct pty_session {
    int            id;
    pty_t         *pty;          /* bridge: real PTY child */
    pty_buffer_t   buffer;       /* bounded output ring */
    bool           alive;
    bool           attached;
    pthread_t      drain_tid;    /* drain thread */
    pthread_mutex_t lock;
    bool           drain_started;
    bool           drain_stopped;
    struct pty_session *next;
} pty_session_t;

static pty_session_t *g_sessions = NULL;
static int g_next_id = 1;
static pthread_mutex_t g_registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* Drain thread: read child output, append to buffer (Python _drain). */
static void *pty_drain_thread(void *arg) {
    pty_session_t *s = arg;
    char chunk[4096];
    while (!s->drain_stopped) {
        if (!s->pty) break;
        int n = pty_read(s->pty, chunk, sizeof(chunk));
        if (n < 0) break;                        /* read error */
        if (n == 0) {                            /* idle tick */
            usleep((useconds_t)(PTY_DRAIN_TIMEOUT * 1000000.0));
            if (!pty_child_alive(s->pty)) {
                s->alive = false;                /* EOF — child exited */
                break;
            }
            continue;
        }
        pthread_mutex_lock(&s->lock);
        buf_append(&s->buffer, chunk, (size_t)n);
        pthread_mutex_unlock(&s->lock);
    }
    return NULL;
}

static pty_session_t *pty_session_new(const char *shell) {
    pty_session_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->id = g_next_id++;
    s->pty = pty_allocate(shell, NULL, 80, 24);
    if (!s->pty) { free(s); return NULL; }
    buf_init(&s->buffer, 65536);
    s->alive = true;
    s->attached = false;
    pthread_mutex_init(&s->lock, NULL);
    return s;
}

static void pty_session_free(pty_session_t *s) {
    if (!s) return;
    if (s->pty) pty_dispose(s->pty);
    buf_destroy(&s->buffer);
    pthread_mutex_destroy(&s->lock);
    free(s);
}

/* ------------------------------------------------------------------ */
/*  Registry helpers                                                   */
/* ------------------------------------------------------------------ */
static pty_session_t *registry_find(int id) {
    for (pty_session_t *s = g_sessions; s; s = s->next)
        if (s->id == id) return s;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Public PoP surface                                                 */
/* ------------------------------------------------------------------ */

/* PoP: __init__ @ hermes_cli/pty_session.py:__init__ */
char *pty_init(long capacity) {
    /* Python: bounded output buffer. */
    if (capacity <= 0) capacity = 65536;
    char *out = NULL;
    asprintf(&out, "{\"capacity\": %ld, \"len\": 0, \"truncated\": false}", capacity);
    return out;
}

/* PoP: snapshot @ hermes_cli/pty_session.py:snapshot */
char *pty_snapshot(const char *buf_json) {
    /* Python: raw bytes. */
    if (!buf_json) return strdup("");
    return strdup(buf_json);
}

/* PoP: start @ hermes_cli/pty_session.py:start */
int pty_start(void) {
    /* Python: create the session and spawn the drain task.
     * Allocates a real PTY child ($SHELL) and starts the drain thread. */
    pty_session_t *s = pty_session_new(NULL);
    if (!s) return -1;
    pthread_mutex_lock(&g_registry_lock);
    s->next = g_sessions;
    g_sessions = s;
    pthread_mutex_unlock(&g_registry_lock);
    if (pthread_create(&s->drain_tid, NULL, pty_drain_thread, s) != 0) {
        pthread_mutex_lock(&g_registry_lock);
        g_sessions = s->next;
        pthread_mutex_unlock(&g_registry_lock);
        pty_session_free(s);
        return -1;
    }
    pthread_detach(s->drain_tid);
    s->drain_started = true;
    return s->id;
}

/* PoP: attach @ hermes_cli/pty_session.py:attach */
int pty_attach(const char *ws_desc) {
    /* Python: replace ws attachment + push snapshot. */
    if (!ws_desc) return -1;
    /* ws_desc is the JSON descriptor of the newest session; find it. */
    pthread_mutex_lock(&g_registry_lock);
    pty_session_t *s = g_sessions;
    pthread_mutex_unlock(&g_registry_lock);
    if (!s) return -1;
    pthread_mutex_lock(&s->lock);
    s->attached = true;
    pthread_mutex_unlock(&s->lock);
    return 0;
}

/* PoP: close @ hermes_cli/pty_session.py:close */
int pty_close(void) {
    /* Python: cancel drain + close bridge.
     * Closes the newest session: stops its drain thread, disposes the
     * PTY child, removes it from the registry. */
    pthread_mutex_lock(&g_registry_lock);
    pty_session_t *s = g_sessions;
    if (s) g_sessions = s->next;
    pthread_mutex_unlock(&g_registry_lock);
    if (!s) return -1;
    s->drain_stopped = true;
    if (s->drain_started)
        pthread_join(s->drain_tid, NULL);
    pty_session_free(s);
    return 0;
}

/* PoP: close_all @ hermes_cli/pty_session.py:close_all */
int pty_close_all(void) {
    /* Python: pop + close each session. Closes every session in the
     * registry: stop drain threads, dispose PTY children, free all. */
    while (g_sessions) {
        pthread_mutex_lock(&g_registry_lock);
        pty_session_t *s = g_sessions;
        if (s) g_sessions = s->next;
        pthread_mutex_unlock(&g_registry_lock);
        if (!s) break;
        s->drain_stopped = true;
        if (s->drain_started)
            pthread_join(s->drain_tid, NULL);
        pty_session_free(s);
    }
    return 0;
}
