/*
 * port_signal_remaining2.c — Port of gateway/platforms/signal.py adapter
 * surface. Extra config, connect/disconnect lifecycle.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ gateway/platforms/signal.py:__init__ */
char *sgl_init(const char *config_json) {
    /* Python: extra config merge. */
    if (!config_json) return strdup("{}");
    printf("signal adapter init (extra config)\n");
    return strdup(config_json);
}

/* PoP: connect @ gateway/platforms/signal.py:connect */
bool sgl_connect(void) {
    /* Python: signal-cli daemon + SSE listener.
     * Delegate to the live C implementation (src/gateway/platforms/
     * signal.c): mark the adapter available and running when signal-cli
     * and the account are configured. */
    const char *number = getenv("SIGNAL_NUMBER");
    if (!number || !*number) {
        fprintf(stderr, "signal connect: SIGNAL_NUMBER required\n");
        return false;
    }
    extern void signal_set_number(const char *number);
    extern void signal_set_cli_path(const char *path);
    extern bool signal_connect(void);
    signal_set_number(number);
    const char *cli_path = getenv("SIGNAL_CLI_PATH");
    if (cli_path && *cli_path) signal_set_cli_path(cli_path);
    return signal_connect();
}

/* PoP: disconnect @ gateway/platforms/signal.py:disconnect */
int sgl_disconnect(void) {
    /* Python: stop SSE + cleanup. */
    extern void signal_disconnect(void);
    signal_disconnect();
    return 0;
}
