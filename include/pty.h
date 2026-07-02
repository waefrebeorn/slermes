/*
 * pty.h — Pseudo-Terminal (PTY) allocation for shell processes
 *
 * Provides cross-platform PTY creation, resizing, and I/O.
 * Replaces node-pty / electron's PTY bridge.
 *
 * Linux:  posix_openpt / grantpt / unlockpt
 * macOS:  posix_openpt (same)
 * Windows: ConPTY (Windows 10+) or fallback to pipe pair
 *
 * PoP: pty_allocate @ electron/main.cjs:terminal:start
 * PoP: pty_resize   @ electron/main.cjs:terminal:resize
 * PoP: pty_read     @ electron/main.cjs:terminal:start
 * PoP: pty_write    @ electron/main.cjs:terminal:write
 * PoP: pty_dispose  @ electron/main.cjs:terminal:dispose
 */

#ifndef PTY_H
#define PTY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define PTY_NAME_MAX 256
#define PTY_READ_BUF 4096
#define PTY_DEFAULT_ROWS 24
#define PTY_DEFAULT_COLS 80
#define PTY_DEFAULT_FLAGS 0

/* ── PTY Handle ────────────────────────────────────────────────────────── */
typedef struct {
    int   master_fd;        /* master PTY fd (read/write) */
    int   slave_fd;         /* slave PTY fd (passed to child) */
    char  name[PTY_NAME_MAX]; /* slave PTY device name */
    int   pid;              /* child process PID */
    int   rows;
    int   cols;
    bool  active;
} pty_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: pty_allocate @ electron/main.cjs:terminal:start */
/* Allocate a new PTY and spawn a shell process.
 * shell: path to shell binary (e.g. "/bin/bash"), NULL for default ($SHELL).
 * env: NULL-terminated array of "KEY=VALUE" strings, NULL for inherited env.
 * cols,rows: initial terminal dimensions.
 * Returns NULL on failure. */
pty_t *pty_allocate(const char *shell, char *const env[], int cols, int rows);

/* PoP: pty_resize @ electron/main.cjs:terminal:resize */
/* Resize an active PTY. Updates kernel-side window size and sends SIGWINCH. */
bool pty_resize(pty_t *pty, int cols, int rows);

/* PoP: pty_read @ electron/main.cjs:terminal:start */
/* Read output from the PTY master (non-blocking internal buffer).
 * buf: output buffer, bufsize: max bytes.
 * Returns bytes read, 0 on no data, -1 on error. */
int pty_read(pty_t *pty, char *buf, size_t bufsize);

/* PoP: pty_write @ electron/main.cjs:terminal:write */
/* Write input to the PTY master (child stdin).
 * Returns bytes written, -1 on error. */
int pty_write(pty_t *pty, const char *buf, size_t len);

/* PoP: pty_dispose @ electron/main.cjs:terminal:dispose */
/* Close PTY and terminate child process. */
void pty_dispose(pty_t *pty);

/* ── Child PID helpers ───────────────────────────────────────────────────── */

/* Get the child PID for a PTY (useful for waitpid/kill). */
int pty_child_pid(const pty_t *pty);

/* Check if the child process is still running. */
bool pty_child_alive(const pty_t *pty);

/* ── PTY name helper ────────────────────────────────────────────────────── */

/* Get the slave PTY device name (for spawning child processes). */
const char *pty_slave_name(const pty_t *pty);

#ifdef __cplusplus
}
#endif

#endif /* PTY_H */
