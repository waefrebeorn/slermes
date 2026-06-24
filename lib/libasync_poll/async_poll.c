/*
 * async_poll.c — General-purpose poll-based async event loop.
 *
 * A lightweight, dependency-free asynchronous event loop using poll().
 * Provides fd monitoring (read/write/error callbacks), timer callbacks,
 * and deferred execution (call_soon pattern). No epoll/kqueue/external deps.
 *
 * Internally tracks entries in a flat array (no malloc per operation).
 * Each entry has: fd, event mask, read/write/error callbacks + userdata.
 * Timers are tracked separately with absolute expiry timestamps.
 *
 * Port of Python: asyncio event loop — poll-based, no external dependencies.
 * MIT License — WuBu Hermes Project
 */

#include "async_poll.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

/* ── Constants ── */
#define ASYNC_POLL_DEFAULT_CAPACITY 64
#define MAX_DEFERRED 64

/* ── Event entry ── */
typedef struct {
    int    fd;                  /* File descriptor to monitor */
    bool   active;              /* true = slot in use */

    /* Callbacks */
    async_poll_read_cb_t  on_read;
    void                 *read_userdata;
    async_poll_write_cb_t on_write;
    void                 *write_userdata;
    async_poll_error_cb_t on_error;
    void                 *error_userdata;
} async_poll_fd_entry_t;

/* ── Timer entry ── */
typedef struct {
    int    timer_id;            /* Unique timer identifier */
    int    interval_ms;         /* Interval in ms */
    bool   repeat;              /* true = repeating timer */
    bool   active;              /* true = slot in use */

    /* Absolute expiry (monotonic clock) */
    struct timespec expiry;

    /* Callback */
    async_poll_timer_cb_t cb;
    void                 *userdata;
} async_poll_timer_entry_t;

/* ── Deferred callback entry ── */
typedef struct {
    async_poll_defer_cb_t cb;
    void                 *userdata;
    bool                  active;
} async_poll_defer_entry_t;

/* ── Event loop struct ── */
struct async_poll_t {
    /* FD monitoring */
    async_poll_fd_entry_t  *fd_entries;
    int                     fd_capacity;
    int                     fd_count;

    /* Timer management */
    async_poll_timer_entry_t *timer_entries;
    int                       timer_capacity;
    int                       timer_count;
    int                       next_timer_id;

    /* Deferred callbacks */
    async_poll_defer_entry_t deferred[MAX_DEFERRED];
    int                       deferred_count;

    /* Loop state */
    bool stopped;
    int  iteration;
};

/* ── Internal: monotonic time in ms ── */
static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}

/* ── Create / Destroy ── */

async_poll_t *async_poll_create(int max_fds) {
    if (max_fds < 1) max_fds = ASYNC_POLL_DEFAULT_CAPACITY;

    async_poll_t *loop = (async_poll_t *)calloc(1, sizeof(async_poll_t));
    if (!loop) return NULL;

    loop->fd_entries = (async_poll_fd_entry_t *)calloc(
        (size_t)max_fds, sizeof(async_poll_fd_entry_t));
    if (!loop->fd_entries) { free(loop); return NULL; }

    loop->timer_entries = (async_poll_timer_entry_t *)calloc(
        (size_t)max_fds, sizeof(async_poll_timer_entry_t));
    if (!loop->timer_entries) {
        free(loop->fd_entries);
        free(loop);
        return NULL;
    }

    loop->fd_capacity = max_fds;
    loop->timer_capacity = max_fds;
    loop->fd_count = 0;
    loop->timer_count = 0;
    loop->next_timer_id = 1;
    loop->deferred_count = 0;
    loop->stopped = false;
    loop->iteration = 0;

    return loop;
}

void async_poll_destroy(async_poll_t *loop) {
    if (!loop) return;
    free(loop->fd_entries);
    free(loop->timer_entries);
    free(loop);
}

void async_poll_stop(async_poll_t *loop) {
    if (loop) loop->stopped = true;
}

bool async_poll_is_stopped(const async_poll_t *loop) {
    return loop && loop->stopped;
}

/* ── Internal: find free fd slot ── */
static int fd_find_free(async_poll_t *loop) {
    for (int i = 0; i < loop->fd_capacity; i++) {
        if (!loop->fd_entries[i].active) return i;
    }
    return -1;
}

/* ── Internal: find fd slot by fd ── */
static int fd_find_by_fd(async_poll_t *loop, int fd) {
    for (int i = 0; i < loop->fd_capacity; i++) {
        if (loop->fd_entries[i].active && loop->fd_entries[i].fd == fd)
            return i;
    }
    return -1;
}

/* ── FD monitoring ── */

bool async_poll_add_reader(async_poll_t *loop, int fd,
                            async_poll_read_cb_t cb, void *userdata) {
    if (!loop || fd < 0 || !cb) return false;

    int slot = fd_find_by_fd(loop, fd);
    if (slot >= 0) {
        /* Already tracked — just update callback */
        loop->fd_entries[slot].on_read = cb;
        loop->fd_entries[slot].read_userdata = userdata;
        return true;
    }

    slot = fd_find_free(loop);
    if (slot < 0) return false;

    loop->fd_entries[slot].fd = fd;
    loop->fd_entries[slot].active = true;
    loop->fd_entries[slot].on_read = cb;
    loop->fd_entries[slot].read_userdata = userdata;
    loop->fd_count++;
    return true;
}

void async_poll_remove_reader(async_poll_t *loop, int fd) {
    if (!loop) return;
    int slot = fd_find_by_fd(loop, fd);
    if (slot < 0) return;
    loop->fd_entries[slot].on_read = NULL;
    loop->fd_entries[slot].read_userdata = NULL;
    /* If no callbacks remain, deactivate the slot */
    if (!loop->fd_entries[slot].on_write && !loop->fd_entries[slot].on_error) {
        loop->fd_entries[slot].active = false;
        loop->fd_count--;
    }
}

bool async_poll_add_writer(async_poll_t *loop, int fd,
                            async_poll_write_cb_t cb, void *userdata) {
    if (!loop || fd < 0 || !cb) return false;

    int slot = fd_find_by_fd(loop, fd);
    if (slot >= 0) {
        loop->fd_entries[slot].on_write = cb;
        loop->fd_entries[slot].write_userdata = userdata;
        return true;
    }

    slot = fd_find_free(loop);
    if (slot < 0) return false;

    loop->fd_entries[slot].fd = fd;
    loop->fd_entries[slot].active = true;
    loop->fd_entries[slot].on_write = cb;
    loop->fd_entries[slot].write_userdata = userdata;
    loop->fd_count++;
    return true;
}

void async_poll_remove_writer(async_poll_t *loop, int fd) {
    if (!loop) return;
    int slot = fd_find_by_fd(loop, fd);
    if (slot < 0) return;
    loop->fd_entries[slot].on_write = NULL;
    loop->fd_entries[slot].write_userdata = NULL;
    if (!loop->fd_entries[slot].on_read && !loop->fd_entries[slot].on_error) {
        loop->fd_entries[slot].active = false;
        loop->fd_count--;
    }
}

bool async_poll_add_error_handler(async_poll_t *loop, int fd,
                                   async_poll_error_cb_t cb, void *userdata) {
    if (!loop || fd < 0 || !cb) return false;

    int slot = fd_find_by_fd(loop, fd);
    if (slot >= 0) {
        loop->fd_entries[slot].on_error = cb;
        loop->fd_entries[slot].error_userdata = userdata;
        return true;
    }

    slot = fd_find_free(loop);
    if (slot < 0) return false;

    loop->fd_entries[slot].fd = fd;
    loop->fd_entries[slot].active = true;
    loop->fd_entries[slot].on_error = cb;
    loop->fd_entries[slot].error_userdata = userdata;
    loop->fd_count++;
    return true;
}

void async_poll_remove_fd(async_poll_t *loop, int fd) {
    if (!loop) return;
    int slot = fd_find_by_fd(loop, fd);
    if (slot >= 0) {
        memset(&loop->fd_entries[slot], 0, sizeof(async_poll_fd_entry_t));
        loop->fd_count--;
    }
}

/* ── Timer management ── */

int async_poll_add_timer(async_poll_t *loop, int interval_ms,
                          bool repeat, async_poll_timer_cb_t cb,
                          void *userdata) {
    if (!loop || !cb) return -1;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < loop->timer_capacity; i++) {
        if (!loop->timer_entries[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int tid = loop->next_timer_id++;

    /* Compute expiry */
    int64_t expiry = now_ms() + interval_ms;
    loop->timer_entries[slot].timer_id = tid;
    loop->timer_entries[slot].interval_ms = interval_ms;
    loop->timer_entries[slot].repeat = repeat;
    loop->timer_entries[slot].active = true;
    loop->timer_entries[slot].expiry.tv_sec = (time_t)(expiry / 1000);
    loop->timer_entries[slot].expiry.tv_nsec = (long)((expiry % 1000) * 1000000);
    loop->timer_entries[slot].cb = cb;
    loop->timer_entries[slot].userdata = userdata;
    loop->timer_count++;
    return tid;
}

bool async_poll_remove_timer(async_poll_t *loop, int timer_id) {
    if (!loop || timer_id < 0) return false;
    for (int i = 0; i < loop->timer_capacity; i++) {
        if (loop->timer_entries[i].active &&
            loop->timer_entries[i].timer_id == timer_id) {
            loop->timer_entries[i].active = false;
            loop->timer_count--;
            return true;
        }
    }
    return false;
}

/* ── Deferred callbacks ── */

bool async_poll_call_soon(async_poll_t *loop,
                           async_poll_defer_cb_t cb, void *userdata) {
    if (!loop || !cb) return false;
    if (loop->deferred_count >= MAX_DEFERRED) return false;
    loop->deferred[loop->deferred_count].cb = cb;
    loop->deferred[loop->deferred_count].userdata = userdata;
    loop->deferred[loop->deferred_count].active = true;
    loop->deferred_count++;
    return true;
}

/* ── Compute timeout for next timer expiry ── */
static int compute_timeout_ms(async_poll_t *loop) {
    if (loop->deferred_count > 0) return 0;  /* Don't wait if deferred pending */

    int64_t now = now_ms();
    int nearest = -1;  /* -1 = no timers */

    for (int i = 0; i < loop->timer_capacity; i++) {
        if (!loop->timer_entries[i].active) continue;
        int64_t expiry = (int64_t)loop->timer_entries[i].expiry.tv_sec * 1000
                       + loop->timer_entries[i].expiry.tv_nsec / 1000000;
        int remaining = (int)(expiry - now);
        if (remaining < 0) remaining = 0;
        if (nearest < 0 || remaining < nearest)
            nearest = remaining;
    }

    return nearest;  /* -1 means wait indefinitely (no timers) */
}

/* ── Fire due timers ── */
static int fire_timers(async_poll_t *loop) {
    int fired = 0;
    if (loop->timer_count == 0) return 0;

    int64_t now = now_ms();

    for (int i = 0; i < loop->timer_capacity; i++) {
        if (!loop->timer_entries[i].active) continue;

        int64_t expiry = (int64_t)loop->timer_entries[i].expiry.tv_sec * 1000
                       + loop->timer_entries[i].expiry.tv_nsec / 1000000;

        if (now >= expiry) {
            /* Timer is due */
            bool rearm = loop->timer_entries[i].cb(
                loop->timer_entries[i].timer_id,
                loop->timer_entries[i].userdata);

            if (rearm && loop->timer_entries[i].repeat && loop->timer_entries[i].active) {
                /* Re-arm: compute next expiry */
                int64_t next = now + loop->timer_entries[i].interval_ms;
                loop->timer_entries[i].expiry.tv_sec = (time_t)(next / 1000);
                loop->timer_entries[i].expiry.tv_nsec = (long)((next % 1000) * 1000000);
            } else {
                /* One-shot or callback returned false — remove */
                loop->timer_entries[i].active = false;
                loop->timer_count--;
            }
            fired++;
        }
    }
    return fired;
}

/* ── Fire deferred callbacks ── */
static int fire_deferred(async_poll_t *loop) {
    int fired = 0;
    for (int i = 0; i < loop->deferred_count; i++) {
        if (!loop->deferred[i].active) continue;
        loop->deferred[i].cb(loop->deferred[i].userdata);
        loop->deferred[i].active = false;
        fired++;
    }
    loop->deferred_count = 0;
    return fired;
}

/* ── Run once ── */
/* Port of Python: asyncio.run_once() — poll + dispatch callbacks + fire timers */

int async_poll_run_once(async_poll_t *loop, int timeout_ms) {
    if (!loop || loop->stopped) return 0;
    if (loop->fd_count == 0 && loop->timer_count == 0 && loop->deferred_count == 0)
        return 0;

    int events = 0;

    /* 1. Fire deferred callbacks first */
    events += fire_deferred(loop);

    /* 2. Build pollfd array from active fd entries */
    struct pollfd *pfds = (struct pollfd *)calloc(
        (size_t)loop->fd_count, sizeof(struct pollfd));
    int *slot_map = (int *)calloc(
        (size_t)loop->fd_count, sizeof(int));
    if (!pfds || !slot_map) {
        free(pfds); free(slot_map);
        return -1;
    }

    int pcount = 0;
    for (int i = 0; i < loop->fd_capacity; i++) {
        if (!loop->fd_entries[i].active) continue;
        if (loop->fd_entries[i].fd < 0) {
            loop->fd_entries[i].active = false;
            loop->fd_count--;
            continue;
        }
        short events_mask = 0;
        if (loop->fd_entries[i].on_read) events_mask |= POLLIN;
        if (loop->fd_entries[i].on_write) events_mask |= POLLOUT;
        if (loop->fd_entries[i].on_error) events_mask |= POLLERR | POLLHUP;
        if (events_mask == 0) continue;

        pfds[pcount].fd = loop->fd_entries[i].fd;
        pfds[pcount].events = events_mask;
        pfds[pcount].revents = 0;
        slot_map[pcount] = i;
        pcount++;
    }

    /* Determine poll timeout */
    int poll_timeout = timeout_ms;
    if (poll_timeout < 0) {
        /* No explicit timeout: use nearest timer, or -1 if no timers */
        poll_timeout = compute_timeout_ms(loop);
    }

    /* 3. poll() */
    int pret = pcount > 0 ? poll(pfds, (nfds_t)pcount, poll_timeout) : 0;

    /* 4. Fire timers (even if poll timed out, timers may be due) */
    events += fire_timers(loop);

    /* 5. Process poll results */
    if (pret > 0 && pfds) {
        for (int p = 0; p < pcount; p++) {
            if (pfds[p].revents == 0) continue;

            int slot = slot_map[p];
            async_poll_fd_entry_t *entry = &loop->fd_entries[slot];
            if (!entry->active) continue;

            int revents = pfds[p].revents;

            /* Error/ hangup */
            if ((revents & (POLLERR | POLLHUP | POLLNVAL)) && entry->on_error) {
                entry->on_error(entry->fd, entry->error_userdata);
                events++;
                continue;
            }

            /* Readable */
            if ((revents & POLLIN) && entry->on_read) {
                entry->on_read(entry->fd, entry->read_userdata);
                events++;
            }

            /* Writable */
            if ((revents & POLLOUT) && entry->on_write) {
                entry->on_write(entry->fd, entry->write_userdata);
                events++;
            }
        }
    }

    free(pfds);
    free(slot_map);

    loop->iteration++;
    return events;
}

/* ── Run forever ── */
/* Port of Python: asyncio.run_forever() */

int async_poll_run(async_poll_t *loop) {
    if (!loop) return 0;
    loop->stopped = false;
    int total = 0;

    while (!loop->stopped) {
        int n = async_poll_run_once(loop, 100);  /* 100ms default tick */
        if (n < 0) break;
        total += n;
        if (n == 0) {
            /* No events — brief sleep to avoid busy-wait */
            struct timespec ts = {0, 10000000};  /* 10ms */
            nanosleep(&ts, NULL);
        }
    }

    return total;
}

/* ── Queries ── */

int async_poll_fd_count(const async_poll_t *loop) {
    return loop ? loop->fd_count : 0;
}

int async_poll_timer_count(const async_poll_t *loop) {
    return loop ? loop->timer_count : 0;
}

int async_poll_capacity(const async_poll_t *loop) {
    return loop ? loop->fd_capacity : 0;
}
