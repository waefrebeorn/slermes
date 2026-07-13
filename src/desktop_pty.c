/* desktop_pty.c -- extracted from src/app_desktop.c (angel-coder monolith split).
 * Self-contained desktop UI/PTY concern module. See app_desktop_internals.h.
 */

#include "app_desktop_internals.h"

void term_launch_pty(void) {
    if (app.term_pty && app.term_pty->active) return;
    app.term_pty = pty_allocate(NULL, NULL, 80, 24);
    if (app.term_pty && app.term_pty->active) {
        app.term_buf_len = 0;
        fprintf(stderr, "PTY launched: PID=%d fd=%d\n", app.term_pty->pid, app.term_pty->master_fd);
    }
}

void term_read_pty(void) {
    if (!app.term_pty || !app.term_pty->active) return;
    char readbuf[4096];
    int n = pty_read(app.term_pty, readbuf, sizeof(readbuf) - 1);
    if (n > 0) {
        readbuf[n] = '\0';
        int new_len = app.term_buf_len + n;
        if (new_len >= (int)sizeof(app.term_buf)) {
            int keep = 16384;
            int start = app.term_buf_len - keep;
            if (start > 0) {
                memmove(app.term_buf, app.term_buf + start, keep);
                app.term_buf_len = keep;
            } else {
                app.term_buf_len = 0;
            }
            new_len = app.term_buf_len + n;
        }
        memcpy(app.term_buf + app.term_buf_len, readbuf, n);
        app.term_buf_len = new_len;
        app.term_buf[app.term_buf_len] = '\0';
        ui.dirty = true;
    }
}

void term_shutdown_pty(void) {
    if (app.term_pty) {
        if (app.term_pty->active) pty_dispose(app.term_pty);
        app.term_pty = NULL;
    }
}

void term_write_pty(const char *data, int len) {
    if (app.term_pty && app.term_pty->active) {
        pty_write(app.term_pty, data, len);
        ui.dirty = true;
    }
}
