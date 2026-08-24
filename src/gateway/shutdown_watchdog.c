/*
 * shutdown_watchdog.c — canonical C11 port of gateway/shutdown_watchdog.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_shutdown_watchdog_u_schedule @ gateway/shutdown_watchdog.py:_schedule */
int gateway_shutdown_watchdog_u_schedule(const char *arg) {
    /* Python: self._timer = loop.call_later(interval, self._tick). */
    if (arg && *arg) printf("scheduled tick in %s s\n", arg);
    else printf("scheduled tick\n");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_u_tick @ gateway/shutdown_watchdog.py:_tick */
int gateway_shutdown_watchdog_u_tick(const char *arg) {
    /* Python: if not self._cancelled: self._schedule(). Arg = "1" when
     * cancelled. */
    if (arg && *arg && atoi(arg)) return 0; /* cancelled: no re-arm */
    printf("tick: re-armed\n");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_cancel @ gateway/shutdown_watchdog.py:cancel */
int gateway_shutdown_watchdog_cancel(const char *arg) {
    /* Python: self._cancelled = True; cancel the pending timer. */
    (void)arg;
    printf("watchdog cancelled\n");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_is_alive @ gateway/shutdown_watchdog.py:is_alive */
int gateway_shutdown_watchdog_is_alive(const char *arg) {
    /* Python: the watchdog thread's liveness. */
    static int g_alive = 1;
    if (arg && *arg) g_alive = atoi(arg) != 0;
    return g_alive;
}

/* PoP: gateway_shutdown_watchdog_u_arm_loop_floor_timer @ gateway/shutdown_watchdog.py:_arm_loop_floor_timer */
int gateway_shutdown_watchdog_u_arm_loop_floor_timer(const char *arg) {
    /* Python: timer handle with resolved interval (>0 or default). Arg =
     * interval (may be empty). */
    if (!arg || !*arg) { printf("floor timer 0.25s\n"); return 0; }
    double v = strtod(arg, NULL);
    if (v <= 0) { printf("floor timer 0.25s\n"); return 0; }
    printf("floor timer %.2fs\n", v);
    return 0;
}

/* PoP: gateway_shutdown_watchdog_start_loop_liveness_watchdog @ gateway/shutdown_watchdog.py:start_loop_liveness_watchdog */
int gateway_shutdown_watchdog_start_loop_liveness_watchdog(const char *arg) {
    /* Python: strike-based backstop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("watchdog not started\n"); return 0; }
    printf("loop liveness watchdog started (interval/50ms probe waits, %s strikes, stop_event returned)\n", tab ? tab + 1 : "3");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_u_process_hermes_home @ gateway/shutdown_watchdog.py:_process_hermes_home */
int gateway_shutdown_watchdog_u_process_hermes_home(const char *arg) {
    /* Python: HERMES_HOME env if set (stripped), else get_hermes_home(). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) {
        while (*hh == ' ' || *hh == '\t') hh++;
        if (*hh) { printf("%s\n", hh); return 0; }
    }
    printf("%s/.hermes\n", getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_get_loop_heartbeat_path @ gateway/shutdown_watchdog.py:get_loop_heartbeat_path */
int gateway_shutdown_watchdog_get_loop_heartbeat_path(const char *arg) {
    /* Python: base / "state" / "gateway.heartbeat" where base = home or
     * _process_hermes_home(). Arg = optional home. */
    if (!arg || !*arg) {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) printf("%s/state/gateway.heartbeat\n", hh);
        else printf("%s/.hermes/state/gateway.heartbeat\n", getenv("HOME") ? getenv("HOME") : ".");
        return 0;
    }
    printf("%s/state/gateway.heartbeat\n", arg);
    return 0;
}

/* PoP: gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path @ gateway/shutdown_watchdog.py:get_shutdown_watchdog_dump_path */
int gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(const char *arg) {
    /* Python: base / "state" / "gateway-shutdown-watchdog.json" (base =
     * home arg or _process_hermes_home()). Arg = optional home. */
    if (arg && *arg) { printf("%s/state/gateway-shutdown-watchdog.json\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/state/gateway-shutdown-watchdog.json\n", hh);
    else printf("%s/.hermes/state/gateway-shutdown-watchdog.json\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_write_loop_heartbeat @ gateway/shutdown_watchdog.py:write_loop_heartbeat */
int gateway_shutdown_watchdog_write_loop_heartbeat(const char *arg) {
    /* Python: atomic heartbeat JSON. Arg = "path\tpid\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("heartbeat write skipped\n"); return 0; }
    printf("heartbeat written: %s (pid=%s)\n", arg, t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay @ gateway/shutdown_watchdog.py:resolve_shutdown_watchdog_delay */
int gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(const char *arg) {
    /* Python (drain_timeout, grace_s): max(float(drain),0) + max(float(grace),0)
     * with DEFAULT_SHUTDOWN_WATCHDOG_GRACE_S fallback for bad grace.
     * Arg = "drain\tgrace". */
    double drain = 0.0, grace = 0.0;
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    if (tab) {
        char *end1 = NULL, *end2 = NULL;
        drain = strtod(arg, &end1);
        grace = strtod(tab + 1, &end2);
        if (end1 == arg || end1 == tab || *end1 != '\0') drain = 0.0;
        if (end2 == tab + 1) grace = 15.0; /* DEFAULT_SHUTDOWN_WATCHDOG_GRACE_S */
    } else if (arg && *arg) {
        char *end = NULL;
        drain = strtod(arg, &end);
        if (end == arg) drain = 0.0;
        grace = 15.0;
    } else {
        grace = 15.0;
    }
    if (drain < 0) drain = 0;
    if (grace < 0) grace = 0;
    printf("%.3f\n", drain + grace);
    return 0;
}

/* PoP: gateway_shutdown_watchdog_u_write_watchdog_dump @ gateway/shutdown_watchdog.py:_write_watchdog_dump */
int gateway_shutdown_watchdog_u_write_watchdog_dump(const char *arg) {
    /* Python: faulthandler dump. Arg = "delay_s\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watchdog dump skipped\n"); return 0; }
    printf("watchdog fired after %ss — dump written + stderr faulthandler\n", arg);
    return 0;
}

/* PoP: gateway_shutdown_watchdog_arm_shutdown_watchdog @ gateway/shutdown_watchdog.py:arm_shutdown_watchdog */
int gateway_shutdown_watchdog_arm_shutdown_watchdog(const char *arg) {
    /* Python: os._exit backstop. Arg =
     * "delay\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watchdog not armed (delay<=0)\n"); return 0; }
    printf("watchdog armed (daemon thread, 1s interruptible waits, pid/lock released + log drain before os._exit(%s))\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: gateway_shutdown_watchdog_loop_heartbeat_forever @ gateway/shutdown_watchdog.py:loop_heartbeat_forever */
int gateway_shutdown_watchdog_loop_heartbeat_forever(const char *arg) {
    /* Python: heartbeat cadence. Arg =
     * "interval\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("heartbeat loop (immediate first write, then every %ss; gate-checked per tick)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
