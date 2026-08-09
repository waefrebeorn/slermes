/*
 * port_hermes_cli_update_lock.h — C11 port of hermes_cli/update_lock.py
 */
#ifndef PORT_HERMES_CLI_UPDATE_LOCK_H
#define PORT_HERMES_CLI_UPDATE_LOCK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_MARKER_MAX_AGE_SECONDS (20 * 60)
#define UL_MARKER_NAME ".hermes-update-in-progress"
#define UL_HANDOFF_PID_ENV "HERMES_UPDATE_HANDOFF_PID"
#define UL_EXIT_CONCURRENT 2

/* PoP: update_marker_path @ hermes_cli/update_lock.py:update_marker_path */
char *ul_update_marker_path(void);

/* PoP: _pid_alive @ hermes_cli/update_lock.py:_pid_alive */
bool ul_pid_alive(int pid);

/* PoP: _handoff_pid @ hermes_cli/update_lock.py:_handoff_pid */
int ul_handoff_pid(void);

/* PoP: _is_ancestor_pid @ hermes_cli/update_lock.py:_is_ancestor_pid */
bool ul_is_ancestor_pid(int pid);

/* UpdateHolder: PoP: UpdateHolder @ hermes_cli/update_lock.py:UpdateHolder */
typedef struct {
    int pid;
    double age_seconds;
} UpdateHolder;

/* PoP: read_live_update @ hermes_cli/update_lock.py:read_live_update */
UpdateHolder *ul_read_live_update(const char *path);

/* PoP: describe_holder @ hermes_cli/update_lock.py:describe_holder */
char *ul_describe_holder(const UpdateHolder *holder);

/* PoP: __init__ @ hermes_cli/update_lock.py:UpdateLock.__init__ */
/* PoP: acquire @ hermes_cli/update_lock.py:UpdateLock.acquire */
/* PoP: release @ hermes_cli/update_lock.py:UpdateLock.release */
/* PoP: __enter__ @ hermes_cli/update_lock.py:UpdateLock.__enter__ */
/* PoP: __exit__ @ hermes_cli/update_lock.py:UpdateLock.__exit__ */
typedef struct {
    char *path;
    bool acquired;
    UpdateHolder *holder;
} UpdateLock;

UpdateLock *ul_update_lock_new(const char *path);
bool ul_update_lock_acquire(UpdateLock *lock);
void ul_update_lock_release(UpdateLock *lock);
bool ul_update_lock_enter(UpdateLock *lock);
void ul_update_lock_exit(UpdateLock *lock);

/* Lifecycle */
void ul_update_lock_free(UpdateLock *lock);
void ul_update_holder_free(UpdateHolder *holder);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_UPDATE_LOCK_H */
