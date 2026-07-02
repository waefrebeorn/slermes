#ifndef ASYNC_POLL_H
#define ASYNC_POLL_H

/*
 * async_poll.h — General-purpose poll-based async event loop.
 *
 * A lightweight, dependency-free asynchronous event loop using poll().
 * Provides fd monitoring (read/write callbacks), timer callbacks,
 * and deferred execution (call_soon). No epoll, kqueue, or external
 * dependencies — pure POSIX poll().
 *
 * Port of Python: asyncio event loop (synchronous/poll-based subset).
 * C implementation: poll()-based multiplexing (proven in websocket_async.c).
 *
 * Usage:
 *   async_poll_t *loop = async_poll_create(64);
 *
 *   // Register fd callback
 *   async_poll_add_reader(loop, fd, on_data, my_data);
 *
 *   // Register timer (fires every 5 seconds)
 *   async_poll_add_timer(loop, 5000, true, on_tick, NULL);
 *
 *   // Run one tick (non-blocking)
 *   int n = async_poll_run_once(loop, 0);
 *
 *   // Run main loop (blocks until stop is called)
 *   async_poll_run(loop);
 *   async_poll_stop(loop);
 *
 *   async_poll_destroy(loop);
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Forward declarations ── */
typedef struct async_poll_t async_poll_t;

/* ── Callback types ── */

/* Called when an fd is readable (POLLIN).
 * fd: the file descriptor that has data.
 * userdata: opaque pointer from registration.
 */
typedef void (*async_poll_read_cb_t)(int fd, void *userdata);

/* Called when an fd is writable (POLLOUT).
 * fd: the file descriptor ready for write.
 * userdata: opaque pointer from registration.
 */
typedef void (*async_poll_write_cb_t)(int fd, void *userdata);

/* Called when an fd has an error (POLLERR/POLLHUP).
 * fd: the file descriptor with error.
 * userdata: opaque pointer from registration.
 */
typedef void (*async_poll_error_cb_t)(int fd, void *userdata);

/* Called when a timer fires.
 * timer_id: identifier for the timer.
 * userdata: opaque pointer from registration.
 * Returns true to re-arm the timer (for repeating timers),
 * or false to fire once and remove.
 */
typedef bool (*async_poll_timer_cb_t)(int timer_id, void *userdata);

/* Called for a deferred callback (call_soon pattern).
 * userdata: opaque pointer from registration.
 */
typedef void (*async_poll_defer_cb_t)(void *userdata);

/* ── Event loop lifecycle ── */

/* Create an event loop with capacity for max_fds tracked fds.
 * max_fds: max concurrent fd monitors + timers (default 64 if <1).
 * Returns NULL on OOM.
 * Port of Python: asyncio.get_event_loop()
 */
async_poll_t *async_poll_create(int max_fds);

/* Destroy the event loop, free all resources.
 * Does NOT close registered fds (caller owns fds).
 * Port of Python: loop.close()
 */
void async_poll_destroy(async_poll_t *loop);

/* Request the event loop to stop (sets a flag checked by async_poll_run).
 * Port of Python: loop.stop()
 */
void async_poll_stop(async_poll_t *loop);

/* Check if the event loop has been requested to stop. */
bool async_poll_is_stopped(const async_poll_t *loop);

/* ── FD monitoring ── */

/* Register an fd for read events.
 * fd: file descriptor to monitor.
 * cb: callback when POLLIN fires.
 * userdata: opaque pointer passed to callback.
 * Returns true on success, false if loop is full.
 * Port of Python: loop.add_reader(fd, callback)
 */
bool async_poll_add_reader(async_poll_t *loop, int fd,
                            async_poll_read_cb_t cb, void *userdata);

/* Remove a read monitor for an fd.
 * Port of Python: loop.remove_reader(fd)
 */
void async_poll_remove_reader(async_poll_t *loop, int fd);

/* Register an fd for write events.
 * Port of Python: loop.add_writer(fd, callback)
 */
bool async_poll_add_writer(async_poll_t *loop, int fd,
                            async_poll_write_cb_t cb, void *userdata);

/* Remove a write monitor for an fd. */
void async_poll_remove_writer(async_poll_t *loop, int fd);

/* Register an error/hangup callback for an fd.
 * Called on POLLERR, POLLHUP, or POLLNVAL.
 */
bool async_poll_add_error_handler(async_poll_t *loop, int fd,
                                   async_poll_error_cb_t cb, void *userdata);

/* Remove all monitors for a specific fd. */
void async_poll_remove_fd(async_poll_t *loop, int fd);

/* ── Timer management ── */

/* Add a timer.
 * interval_ms: interval in milliseconds.
 * repeat: true for repeating timer, false for one-shot.
 * cb: callback fired when timer expires.
 * userdata: opaque pointer.
 * Returns timer_id (>=0) on success, -1 on failure.
 * Port of Python: loop.call_later(delay, callback)
 *   or loop.call_soon(callback) with interval_ms=0, repeat=false
 */
int async_poll_add_timer(async_poll_t *loop, int interval_ms,
                          bool repeat, async_poll_timer_cb_t cb,
                          void *userdata);

/* Remove a timer by id.
 * Returns true if timer was found and removed.
 * Port of Python: timer.cancel()
 */
bool async_poll_remove_timer(async_poll_t *loop, int timer_id);

/* ── Deferred callbacks (call_soon pattern) ── */

/* Schedule a callback to run on the next iteration.
 * Port of Python: loop.call_soon(callback)
 * Returns true on success.
 */
bool async_poll_call_soon(async_poll_t *loop,
                           async_poll_defer_cb_t cb, void *userdata);

/* ── Running the loop ── */

/* Run the event loop until async_poll_stop() is called.
 * Calls async_poll_run_once() in a loop.
 * Port of Python: loop.run_forever()
 * Returns the total number of events processed.
 */
int async_poll_run(async_poll_t *loop);

/* Run a single iteration of the event loop.
 * timeout_ms: max milliseconds to poll (0 = non-blocking, -1 = indefinite).
 * Returns number of events handled, 0 on timeout, -1 on error.
 * Port of Python: loop.run_once()
 */
int async_poll_run_once(async_poll_t *loop, int timeout_ms);

/* ── Queries ── */

/* Get number of registered fd monitors. */
int async_poll_fd_count(const async_poll_t *loop);

/* Get number of active timers. */
int async_poll_timer_count(const async_poll_t *loop);

/* Get total capacity. */
int async_poll_capacity(const async_poll_t *loop);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_POLL_H */
