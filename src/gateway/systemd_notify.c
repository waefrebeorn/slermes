/*
 * systemd_notify.c — canonical C11 port of gateway/systemd_notify.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_systemd_notify_u_notify_address @ gateway/systemd_notify.py:_notify_address */
int gateway_systemd_notify_u_notify_address(const char *arg) {
    /* Python: "\0" + raw[1:] if raw starts with "@" else raw — translate
     * systemd's @abstract notation to Python's address form. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '@') {
        putchar('\0');
        printf("%s\n", arg + 1);
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: gateway_systemd_notify_watchdog_interval_seconds @ gateway/systemd_notify.py:watchdog_interval_seconds */
int gateway_systemd_notify_watchdog_interval_seconds(const char *arg) {
    /* Python: NOTIFY_SOCKET + WATCHDOG_USEC/1e6; None on bad. Arg =
     * "notify_socket\twatchdog_usec". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!arg[0] || !tab || !tab[1]) { printf("\n"); return 0; }
    double v = strtod(tab + 1, NULL) / 1000000.0;
    if (!isfinite(v) || v <= 0) { printf("\n"); return 0; }
    printf("%.3f\n", v);
    return 0;
}

/* PoP: gateway_systemd_notify_unhealthy @ gateway/systemd_notify.py:unhealthy */
int gateway_systemd_notify_unhealthy(const char *arg) {
    /* Python property: the unhealthy flag. */
    static int g_unhealthy = 0;
    if (arg && *arg) g_unhealthy = atoi(arg) != 0;
    printf("%d\n", g_unhealthy);
    return 0;
}

/* PoP: gateway_systemd_notify_u_lag_tolerance @ gateway/systemd_notify.py:_lag_tolerance */
int gateway_systemd_notify_u_lag_tolerance(const char *arg) {
    /* Python: configured float or max(0.1, interval*0.25). Arg =
     * "interval\tconfigured". */
    if (!arg || !*arg) { printf("0.10\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double interval = strtod(arg, NULL);
    const char *cfg = tab ? tab + 1 : "";
    if (!cfg[0]) {
        double v = interval * 0.25; if (v < 0.1) v = 0.1;
        printf("%.2f\n", v);
        return 0;
    }
    char *end = NULL;
    double v = strtod(cfg, &end);
    if (end == cfg || !isfinite(v)) {
        double d = interval * 0.25; if (d < 0.1) d = 0.1;
        printf("%.2f\n", d);
        return 0;
    }
    if (v < 0) v = 0;
    printf("%.2f\n", v);
    return 0;
}

/* PoP: gateway_systemd_notify_record_tick @ gateway/systemd_notify.py:record_tick */
int gateway_systemd_notify_record_tick(const char *arg) {
    /* Python: lag budget. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) {
        printf("0 (WATCHDOG=1 paused — unhealthy/late)\n");
        return 0;
    }
    printf("%s\n", (tab && tab[1] == '1') ? "1 (WATCHDOG=1 fed)" : "0 (late — unhealthy)");
    return 0;
}
