#ifndef SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C
#define SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "../process_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

/* PoP: poll @ tools/process_registry.py:ProcessRegistry.poll */
/* Poll the environment — check if the child process is still running and
 * collect any new output. The ctx is a ProcessSession pointer. */
void env_poll(void* ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "env_poll: null context");
        return;
    }
    ProcessSession *session = (ProcessSession *)ctx;

    /* Check if the process is still running */
    if (session->running && session->pid > 0) {
        int status = 0;
        pid_t ret = waitpid(session->pid, &status, WNOHANG);
        if (ret == session->pid) {
            /* Process exited */
            session->running = false;
            session->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            hermes_log(LOG_INFO, "port",
                "env_poll: process %d exited with code %d",
                session->pid, session->exit_code);
        } else if (ret == 0) {
            /* Still running — try to read any available output */
            /* The output collection is handled by the background reader thread
             * in process_registry.c, so we just update the timestamp here. */
            hermes_log(LOG_DEBUG, "port",
                "env_poll: process %d still running", session->pid);
        } else {
            /* Error — process may have been killed externally */
            session->running = false;
            session->exit_code = -1;
            hermes_log(LOG_WARNING, "port",
                "env_poll: waitpid error for pid %d", session->pid);
        }
    }
}

/* PoP: kill @ tools/process_registry.py:ProcessRegistry.kill */
/* Kill the environment's child process. Sends SIGTERM first, then SIGKILL
 * after a short delay if the process doesn't exit. */
void env_kill(void* ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "env_kill: null context");
        return;
    }
    ProcessSession *session = (ProcessSession *)ctx;

    if (!session->running || session->pid <= 0) {
        hermes_log(LOG_DEBUG, "port",
            "env_kill: process not running (pid=%d)", session->pid);
        return;
    }

    /* Send SIGTERM first for graceful shutdown */
    hermes_log(LOG_INFO, "port",
        "env_kill: sending SIGTERM to pid %d", session->pid);
    kill(session->pid, SIGTERM);

    /* Wait up to 2 seconds for graceful exit */
    for (int i = 0; i < 20; i++) {
        int status = 0;
        pid_t ret = waitpid(session->pid, &status, WNOHANG);
        if (ret == session->pid) {
            session->running = false;
            session->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            hermes_log(LOG_INFO, "port",
                "env_kill: process %d exited after SIGTERM", session->pid);
            return;
        }
        if (ret < 0) {
            /* Process already gone */
            session->running = false;
            return;
        }
        usleep(100000); /* 100ms */
    }

    /* Still running — send SIGKILL */
    hermes_log(LOG_WARNING, "port",
        "env_kill: SIGTERM didn't work, sending SIGKILL to pid %d", session->pid);
    kill(session->pid, SIGKILL);

    /* Reap the zombie */
    int status = 0;
    waitpid(session->pid, &status, 0);
    session->running = false;
    session->exit_code = -1;
}

#endif /* SRC_TOOLS_PORT_ENVIRONMENTS_BASE_C */
