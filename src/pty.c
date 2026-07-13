/*
 * pty.c — Pseudo-Terminal (PTY) allocation for shell processes
 *
 * Linux/macOS: posix_openpt / grantpt / unlockpt / fork
 * Windows:      CreatePipe + ConPTY (stub for now, pipe fallback)
 *
 * PoP: pty_allocate @ electron/main.cjs:terminal:start
 * PoP: pty_resize   @ electron/main.cjs:terminal:resize
 * PoP: pty_read     @ electron/main.cjs:terminal:start
 * PoP: pty_write    @ electron/main.cjs:terminal:write
 * PoP: pty_dispose  @ electron/main.cjs:terminal:dispose
 */

#include "pty.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifndef _WIN32
#include <pty.h>       /* openpty on some systems */
#endif

#define PTY_MAGIC 0x50545931  /* "PTY1" */

/* ── Internal helpers ────────────────────────────────────────────────────── */

static const char *default_shell(void) {
    const char *sh = getenv("SHELL");
    if (sh && *sh) return sh;
    return "/bin/sh";
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void pty_setup_slave(int slave_fd, int master_fd) {
    /* Create new session, set as session leader */
    setsid();

    /* Set the slave as the controlling terminal */
#ifdef TIOCSCTTY
    ioctl(slave_fd, TIOCSCTTY, 0);
#endif

    /* Redirect stdin/stdout/stderr to slave */
    dup2(slave_fd, STDIN_FILENO);
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);

    /* Close master fd in child */
    if (slave_fd > STDERR_FILENO)
        close(master_fd);
}

/* PoP: pty_allocate @ electron/main.cjs:terminal:start */
/* PoP: pty_bridge__enter @ hermes_cli/pty_bridge.py:__enter__ */
/* Context-manager enter: spawns the PTY and returns the bridge (self). */
pty_t *pty_allocate(const char *shell, char *const env[], int cols, int rows) {
    pty_t *pty = calloc(1, sizeof(pty_t));
    if (!pty) {
        fprintf(stderr, "PTY calloc failed");
        return NULL;
    }

    pty->master_fd = -1;
    pty->slave_fd  = -1;
    pty->pid       = -1;
    pty->rows      = rows > 0 ? rows : PTY_DEFAULT_ROWS;
    pty->cols      = cols > 0 ? cols : PTY_DEFAULT_COLS;
    pty->active    = false;

#if defined(_WIN32)
    /* Windows: pipe stub — full ConPTY requires Win10+ APIs */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        hermes_log("PTY pipe creation failed: %s", strerror(errno));
        free(pty);
        return NULL;
    }
    pty->master_fd = pipefd[0];
    pty->slave_fd  = pipefd[1];
    set_nonblocking(pty->master_fd);
    strncpy(pty->name, "win32-pipe", sizeof(pty->name) - 1);
#else
    /* Linux/macOS: use openpty (combines posix_openpt + grantpt + unlockpt + ptsname) */
    int master, slave;
    char name_buf[PTY_NAME_MAX];

    if (openpty(&master, &slave, name_buf, NULL, NULL) != 0) {
        hermes_log("openpty() failed: %s", strerror(errno));
        free(pty);
        return NULL;
    }

    pty->master_fd = master;
    pty->slave_fd  = slave;
    strncpy(pty->name, name_buf, sizeof(pty->name) - 1);

    /* Set initial window size */
    struct winsize ws;
    ws.ws_row    = pty->rows;
    ws.ws_col    = pty->cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ioctl(pty->master_fd, TIOCSWINSZ, &ws);

    set_nonblocking(pty->master_fd);

    /* Fork child process */
    pid_t pid = fork();
    if (pid < 0) {
        hermes_log("PTY fork failed: %s", strerror(errno));
        close(pty->master_fd);
        close(pty->slave_fd);
        free(pty);
        return NULL;
    }

    if (pid == 0) {
        /* Child process */
        close(pty->master_fd);
        pty_setup_slave(slave, master);

        /* Set terminal type */
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        const char *exec_shell = shell ? shell : default_shell();

        /* Apply custom env if provided */
        if (env) {
            for (int i = 0; env[i]; i++) {
                char *eq = strchr(env[i], '=');
                if (eq) {
                    size_t klen = (size_t)(eq - env[i]);
                    char key[256];
                    strncpy(key, env[i], klen < sizeof(key) - 1 ? klen : sizeof(key) - 1);
                    key[klen < sizeof(key) - 1 ? klen : sizeof(key) - 1] = '\0';
                    setenv(key, eq + 1, 1);
                }
            }
        }

        execl(exec_shell, exec_shell, "-l", (char *)NULL);
        /* If execl fails, try /bin/sh */
        execl("/bin/sh", "/bin/sh", (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(slave);
    pty->slave_fd = -1;  /* parent doesn't need slave */
#endif

    pty->pid = pid;
    pty->active = true;

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "PTY allocated: master=%d pid=%d '%s' %dx%d",
             pty->master_fd, pty->pid, pty->name, pty->cols, pty->rows);
    fprintf(stderr, "%s", logbuf);

    return pty;
}

/* PoP: pty_resize @ electron/main.cjs:terminal:resize */
bool pty_resize(pty_t *pty, int cols, int rows) {
    if (!pty || !pty->active) return false;

    pty->cols = cols;
    pty->rows = rows;

#if !defined(_WIN32)
    struct winsize ws;
    ws.ws_row    = rows;
    ws.ws_col    = cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    if (ioctl(pty->master_fd, TIOCSWINSZ, &ws) != 0) {
        hermes_log("PTY TIOCSWINSZ failed: %s", strerror(errno));
        return false;
    }

    /* Send SIGWINCH to child process */
    if (pty->pid > 0) {
        kill(pty->pid, SIGWINCH);
    }
#endif

    return true;
}

/* PoP: pty_read @ electron/main.cjs:terminal:start */
int pty_read(pty_t *pty, char *buf, size_t bufsize) {
    if (!pty || !pty->active || pty->master_fd < 0) return -1;

    ssize_t n = read(pty->master_fd, buf, bufsize);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        hermes_log("PTY read error: %s", strerror(errno));
        return -1;
    }
    if (n == 0) {
        pty->active = false;
        return 0;
    }
    return (int)n;
}

/* PoP: pty_write @ electron/main.cjs:terminal:write */
int pty_write(pty_t *pty, const char *buf, size_t len) {
    if (!pty || !pty->active || pty->master_fd < 0) return -1;

    ssize_t written = write(pty->master_fd, buf, len);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        hermes_log("PTY write error: %s", strerror(errno));
        return -1;
    }
    return (int)written;
}

/* PoP: pty_dispose @ electron/main.cjs:terminal:dispose */
/* PoP: pty_bridge__exit @ hermes_cli/pty_bridge.py:__exit__ */
/* Context-manager exit: closes the PTY (Python __exit__ delegates to close()). */
void pty_dispose(pty_t *pty) {
    if (!pty) return;

    pty->active = false;

    if (pty->pid > 0) {
        /* Try graceful termination first */
        kill(pty->pid, SIGHUP);
        /* Give it a moment, then SIGKILL if needed */
        int status = 0;
        pid_t result = waitpid(pty->pid, &status, WNOHANG);
        if (result == 0) {
            /* Still running — force kill */
            kill(pty->pid, SIGKILL);
            waitpid(pty->pid, &status, 0);
        }
        pty->pid = -1;
    }

    if (pty->master_fd >= 0) {
        close(pty->master_fd);
        pty->master_fd = -1;
    }

    if (pty->slave_fd >= 0) {
        close(pty->slave_fd);
        pty->slave_fd = -1;
    }

    free(pty);
}

int pty_child_pid(const pty_t *pty) {
    return pty ? pty->pid : -1;
}

bool pty_child_alive(const pty_t *pty) {
    if (!pty || pty->pid <= 0) return false;
    int status = 0;
    pid_t result = waitpid(pty->pid, &status, WNOHANG);
    return result == 0;
}

const char *pty_slave_name(const pty_t *pty) {
    return pty ? pty->name : NULL;
}
