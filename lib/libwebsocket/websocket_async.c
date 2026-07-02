/*
 * websocket_async.c — Asynchronous WebSocket event loop for C.
 *
 * Poll-based multiplexing layer over the synchronous libwebsocket.
 * Implements ws_async_poll() which checks all registered server listen fds
 * and connection fds for activity, then fires the appropriate callbacks.
 *
 * Architecture:
 *   ws_async_t tracks an array of pollfd structs + metadata arrays.
 *   ws_async_poll() calls poll() on all fds, then iterates results:
 *     - Server listen fd readable → ws_server_accept() → connect callback
 *     - Connection fd readable → ws_recv() → message callback
 *     - Connection fd error/closed → disconnect callback + cleanup
 *
 * Port of Python: asyncio event loop — poll-based (no epoll/kqueue dependency).
 * MIT License — WuBu Hermes Project
 */

#include "websocket_async.h"
#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>

/* ── Event loop struct ── */

/* Entry for a tracked fd. Unified for both servers and connections. */
typedef struct {
    int   fd;                   /* The file descriptor to poll */
    bool  is_server;            /* true = server listen fd, false = connection fd */
    union {
        ws_server_t *server;    /* valid when is_server == true */
        ws_t        *ws;        /* valid when is_server == false */
    } ptr;
    bool  active;               /* false = slot is free */
} ws_async_entry_t;

struct ws_async_t {
    ws_async_entry_t  *entries;        /* Array of tracked fds */
    int                capacity;       /* Max entries (allocated size) */
    int                count;          /* Active entries */

    /* Callbacks */
    ws_async_connect_cb_t     on_connect;
    void                     *connect_userdata;
    ws_async_message_cb_t     on_message;
    void                     *message_userdata;
    ws_async_disconnect_cb_t  on_disconnect;
    void                     *disconnect_userdata;
};

/* ── Create / Destroy ── */

ws_async_t *ws_async_create(int max_connections) {
    if (max_connections < 1) max_connections = 16;

    ws_async_t *ev = (ws_async_t *)calloc(1, sizeof(ws_async_t));
    if (!ev) return NULL;

    ev->entries = (ws_async_entry_t *)calloc((size_t)max_connections, sizeof(ws_async_entry_t));
    if (!ev->entries) {
        free(ev);
        return NULL;
    }

    ev->capacity = max_connections;
    ev->count = 0;
    return ev;
}

void ws_async_destroy(ws_async_t *ev) {
    if (!ev) return;

    /* Close all tracked connections (but not servers) */
    for (int i = 0; i < ev->capacity; i++) {
        if (!ev->entries[i].active) continue;
        if (!ev->entries[i].is_server) {
            ws_close(ev->entries[i].ptr.ws);
        }
        /* Servers are owned by the caller — do NOT close them here */
        ev->entries[i].active = false;
    }

    free(ev->entries);
    free(ev);
}

/* ── Callback registration ── */

void ws_async_set_connect_cb(ws_async_t *ev, ws_async_connect_cb_t cb, void *userdata) {
    if (!ev) return;
    ev->on_connect = cb;
    ev->connect_userdata = userdata;
}

void ws_async_set_message_cb(ws_async_t *ev, ws_async_message_cb_t cb, void *userdata) {
    if (!ev) return;
    ev->on_message = cb;
    ev->message_userdata = userdata;
}

void ws_async_set_disconnect_cb(ws_async_t *ev, ws_async_disconnect_cb_t cb, void *userdata) {
    if (!ev) return;
    ev->on_disconnect = cb;
    ev->disconnect_userdata = userdata;
}

/* ── Internal: find free slot or -1 ── */

static int find_free_slot(ws_async_t *ev) {
    for (int i = 0; i < ev->capacity; i++) {
        if (!ev->entries[i].active) return i;
    }
    return -1;
}

/* ── Internal: find slot by fd or -1 ── */

static int find_slot_by_fd(const ws_async_t *ev, int fd) {
    if (fd < 0) return -1;
    for (int i = 0; i < ev->capacity; i++) {
        if (ev->entries[i].active && ev->entries[i].fd == fd)
            return i;
    }
    return -1;
}

/* ── Server management ── */

bool ws_async_add_server(ws_async_t *ev, ws_server_t *server) {
    if (!ev || !server) return false;

    int fd = ws_server_get_fd(server);
    if (fd < 0) return false;

    /* Don't add duplicates */
    if (find_slot_by_fd(ev, fd) >= 0) return true;

    int slot = find_free_slot(ev);
    if (slot < 0) return false;

    ev->entries[slot].fd = fd;
    ev->entries[slot].is_server = true;
    ev->entries[slot].ptr.server = server;
    ev->entries[slot].active = true;
    ev->count++;
    return true;
}

void ws_async_remove_server(ws_async_t *ev, ws_server_t *server) {
    if (!ev || !server) return;
    int fd = ws_server_get_fd(server);
    int slot = find_slot_by_fd(ev, fd);
    if (slot >= 0) {
        ev->entries[slot].active = false;
        ev->count--;
    }
}

/* ── Connection management ── */

bool ws_async_add_connection(ws_async_t *ev, ws_t *ws) {
    if (!ev || !ws) return false;

    int fd = ws_get_fd(ws);
    if (fd < 0) return false;

    /* Don't add duplicates */
    if (find_slot_by_fd(ev, fd) >= 0) return true;

    int slot = find_free_slot(ev);
    if (slot < 0) return false;

    ev->entries[slot].fd = fd;
    ev->entries[slot].is_server = false;
    ev->entries[slot].ptr.ws = ws;
    ev->entries[slot].active = true;
    ev->count++;
    return true;
}

void ws_async_remove_connection(ws_async_t *ev, ws_t *ws) {
    if (!ev || !ws) return;
    int fd = ws_get_fd(ws);
    int slot = find_slot_by_fd(ev, fd);
    if (slot >= 0) {
        ev->entries[slot].active = false;
        ev->count--;
    }
}

/* ── Connection count ── */

int ws_async_connection_count(const ws_async_t *ev) {
    if (!ev) return 0;
    int count = 0;
    for (int i = 0; i < ev->capacity; i++) {
        if (ev->entries[i].active && !ev->entries[i].is_server)
            count++;
    }
    return count;
}

bool ws_async_has_servers(const ws_async_t *ev) {
    if (!ev) return false;
    for (int i = 0; i < ev->capacity; i++) {
        if (ev->entries[i].active && ev->entries[i].is_server)
            return true;
    }
    return false;
}

/* ── Poll event loop ── */
/* Port of Python: asyncio.run_once() — poll + dispatch callbacks */

int ws_async_poll(ws_async_t *ev, int timeout_ms) {
    if (!ev || ev->count == 0) return 0;

    /* Build pollfd array from active entries */
    int poll_count = 0;
    struct pollfd *pfds = (struct pollfd *)calloc((size_t)ev->count, sizeof(struct pollfd));
    int *slot_map = (int *)calloc((size_t)ev->count, sizeof(int));
    if (!pfds || !slot_map) {
        free(pfds);
        free(slot_map);
        return -1;
    }

    for (int i = 0; i < ev->capacity; i++) {
        if (!ev->entries[i].active) continue;
        if (ev->entries[i].fd < 0) {
            /* Stale fd — mark inactive */
            ev->entries[i].active = false;
            ev->count--;
            continue;
        }
        pfds[poll_count].fd = ev->entries[i].fd;
        pfds[poll_count].events = POLLIN;
        pfds[poll_count].revents = 0;
        slot_map[poll_count] = i;
        poll_count++;
    }

    if (poll_count == 0) {
        free(pfds);
        free(slot_map);
        return 0;
    }

    /* Poll */
    int ret = poll(pfds, (nfds_t)poll_count, timeout_ms);
    if (ret <= 0) {
        free(pfds);
        free(slot_map);
        return ret;  /* 0 = timeout, -1 = error */
    }

    int events_handled = 0;

    /* Process results */
    for (int p = 0; p < poll_count; p++) {
        if (pfds[p].revents == 0) continue;

        int slot = slot_map[p];
        ws_async_entry_t *entry = &ev->entries[slot];

        if (!entry->active) continue;  /* Slot was freed during callback */

        int revents = pfds[p].revents;

        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            /* Connection/server error or hangup */
            if (!entry->is_server && ev->on_disconnect) {
                ev->on_disconnect(entry->ptr.ws, ev->disconnect_userdata);
            }
            /* For connections, close and remove */
            if (!entry->is_server) {
                ws_close(entry->ptr.ws);
            }
            entry->active = false;
            ev->count--;
            events_handled++;
            continue;
        }

        if (!(revents & POLLIN)) continue;

        if (entry->is_server) {
            /* New incoming connection on server listen fd */
            ws_t *new_ws = ws_server_accept(entry->ptr.server, 0);
            if (new_ws) {
                /* Add to event loop */
                int new_slot = find_free_slot(ev);
                if (new_slot >= 0) {
                    ev->entries[new_slot].fd = ws_get_fd(new_ws);
                    ev->entries[new_slot].is_server = false;
                    ev->entries[new_slot].ptr.ws = new_ws;
                    ev->entries[new_slot].active = true;
                    ev->count++;
                }
                /* Fire connect callback */
                if (ev->on_connect) {
                    ev->on_connect(new_ws, ev->connect_userdata);
                }
                events_handled++;
            }
        } else {
            /* Data on a connection */
            ws_frame_t frame;
            int r = ws_recv(entry->ptr.ws, &frame, 0);
            if (r > 0) {
                if (ev->on_message) {
                    ev->on_message(entry->ptr.ws, &frame, ev->message_userdata);
                }
                ws_frame_free(&frame);
                events_handled++;
            } else if (r < 0) {
                /* Connection closed or error */
                if (ev->on_disconnect) {
                    ev->on_disconnect(entry->ptr.ws, ev->disconnect_userdata);
                }
                ws_close(entry->ptr.ws);
                entry->active = false;
                ev->count--;
                events_handled++;
            }
            /* r == 0 means no data on non-blocking recv — happens after signaling wait */
        }
    }

    free(pfds);
    free(slot_map);
    return events_handled;
}
