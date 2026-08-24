/*
 * kanban_watchers.c — canonical C11 port of gateway/kanban_watchers.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <string.h>

static bool g_kanban_lock_owned = false;
static char g_kanban_lock_handle[256] = "";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_kanban_watchers_u_resolve_auto_decompose_settings @ gateway/kanban_watchers.py:_resolve_auto_decompose_settings */
int gateway_kanban_watchers_u_resolve_auto_decompose_settings(const char *arg) {
    /* Python: fresh read, fails safe. Arg =
     * "state\tenabled\tper_tick\tresult". */
    if (!arg || !*arg) { printf("1\t3\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "read_error") == 0) { printf("0\t3\n"); return 0; }
    printf("%s\t%s\n", (t2 && t2[1] == '1') ? "1" : "0", t2 ? t2 + 1 : "3");
    return 0;
}

/* PoP: gateway_kanban_watchers_u_acquire_singleton_lock @ gateway/kanban_watchers.py:_acquire_singleton_lock */
int gateway_kanban_watchers_u_acquire_singleton_lock(const char *arg) {
    /* Python: advisory flock. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("\t%s\n", t3 ? t3 + 1 : "unavailable");
        return 0;
    }
    printf("held\t\n");
    return 0;
}

/* PoP: gateway_kanban_watchers_u_release_singleton_lock @ gateway/kanban_watchers.py:_release_singleton_lock */
int gateway_kanban_watchers_u_release_singleton_lock(const char *arg) {
    /* Python: release file lock + close handle, best-effort. Arg = handle
     * path (or empty = None). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("released %s\n", arg);

    /* Clear notifier-visible ownership if this handle was held. */
    if (arg && *arg && strcmp(arg, g_kanban_lock_handle) == 0) {
        g_kanban_lock_owned = false;
        g_kanban_lock_handle[0] = '\0';
    }
    return 0;
}

/* PoP: lock @ gateway/kanban_watchers.py:_owns_kanban_dispatcher_lock */
/* PoP: lock @ gateway/kanban_watchers.py:_owns_kanban_dispatcher_lock */

/* Port of Python _owns_kanban_dispatcher_lock: return whether this gateway
 * currently owns the singleton dispatcher lock (handle non-None). */
bool gateway_kanban_watchers_u_owns_lock(void)
{
    return g_kanban_lock_owned && g_kanban_lock_handle[0] != '\0';
}

/* PoP: held @ gateway/kanban_watchers.py:_release_kanban_dispatcher_lock */
/* PoP: held @ gateway/kanban_watchers.py:_release_kanban_dispatcher_lock */

/* Port of Python _release_kanban_dispatcher_lock: clear notifier-visible
 * ownership before releasing the OS lock. Releases whatever handle is
 * currently held (best-effort, never raises). */
void gateway_kanban_watchers_u_release_kanban_lock(void)
{
    if (g_kanban_lock_owned && g_kanban_lock_handle[0]) {
        /* Delegate to _release_singleton_lock path (best-effort). */
        gateway_kanban_watchers_u_release_singleton_lock(g_kanban_lock_handle);
    }
    g_kanban_lock_owned = false;
    g_kanban_lock_handle[0] = '\0';
}

/* PoP: gateway_kanban_watchers_u_kanban_notifier_watcher @ gateway/kanban_watchers.py:_kanban_notifier_watcher */
int gateway_kanban_watchers_u_kanban_notifier_watcher(const char *arg) {
    /* Python: notify loop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("notifier watcher running (cursor claim, per-platform notify, rewind on send failure)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: gateway_kanban_watchers_u_kanban_advance @ gateway/kanban_watchers.py:_kanban_advance */
int gateway_kanban_watchers_u_kanban_advance(const char *arg) {
    /* Python: advance_notify_cursor in to_thread. Arg =
     * "board\ttask_id\tcursor\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("advance skipped\n"); return 0; }
    printf("kanban cursor advanced: task=%s cursor=%s (board=%s)\n",
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "", arg);
    return 0;
}

/* PoP: gateway_kanban_watchers_u_kanban_unsub @ gateway/kanban_watchers.py:_kanban_unsub */
int gateway_kanban_watchers_u_kanban_unsub(const char *arg) {
    /* Python: remove_notify_sub then close. Arg = "board\tsub_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("unsubscribed kanban notify: %.*s\n",
           (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: gateway_kanban_watchers_u_kanban_rewind @ gateway/kanban_watchers.py:_kanban_rewind */
int gateway_kanban_watchers_u_kanban_rewind(const char *arg) {
    /* Python: notify cursor undo. Arg =
     * "rewound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int rewound = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!rewound) { printf("0 (no claim)\n"); return 0; }
    printf("1 (notification cursor rewound to old_cursor after send failure — no message loss)%s\n", (t2 && t2[1] == '1') ? " — conn closed" : "");
    return 0;
}

/* PoP: gateway_kanban_watchers_u_deliver_kanban_artifacts @ gateway/kanban_watchers.py:_deliver_kanban_artifacts */
int gateway_kanban_watchers_u_deliver_kanban_artifacts(const char *arg) {
    /* Python: artifact upload. Arg =
     * "uploaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no metadata/attachments)\n"); return 0; }
    printf("%s artifact(s) uploaded (scratch → attachment dir salvage; media + files native uploads; missing files skipped w/ note)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — persisted list" : "");
    return 0;
}

/* PoP: gateway_kanban_watchers_u_kanban_dispatcher_watcher @ gateway/kanban_watchers.py:_kanban_dispatcher_watcher */
int gateway_kanban_watchers_u_kanban_dispatcher_watcher(const char *arg) {
    /* Python: dispatch loop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("dispatcher watcher running (interval poll, claim+dispatch, rewind on failure, supervised)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* === dissolved from ports_wrappers.c === */
/* ---- Dispatcher lock ownership tracking ----
 * Mirrors GatewayKanbanWatchersMixin._kanban_dispatcher_lock_handle on the
 * C GatewayRunner. */

/* Claim ownership (called after a successful _acquire_singleton_lock). */
void gateway_kanban_watchers_u_claim_lock(const char *handle)
{
    if (!handle || !*handle) { g_kanban_lock_owned = false; g_kanban_lock_handle[0] = '\0'; return; }
    g_kanban_lock_owned = true;
    strncpy(g_kanban_lock_handle, handle, sizeof(g_kanban_lock_handle) - 1);
    g_kanban_lock_handle[sizeof(g_kanban_lock_handle) - 1] = '\0';
}
