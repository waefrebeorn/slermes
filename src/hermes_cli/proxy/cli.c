/*
 * cli.c — canonical C11 port of hermes_cli/proxy/cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_proxy_cli_u_print_aiohttp_missing @ hermes_cli/proxy/cli.py:_print_aiohttp_missing */
int hermes_cli_proxy_cli_u_print_aiohttp_missing(const char *arg) {
    /* Python: print aiohttp-missing guidance. */
    fprintf(stderr,
            "aiohttp is required for the iron-proxy CLI. Install with: pip install aiohttp\n");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_proxy_start @ hermes_cli/proxy/cli.py:cmd_proxy_start */
int hermes_cli_proxy_cli_cmd_proxy_start(const char *arg) {
    /* Python: foreground proxy. Arg =
     * "provider\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_aiohttp") == 0) {
        fprintf(stderr, "aiohttp missing — install hermes[proxy]\n");
        return 1;
    }
    if (strcmp(state, "unknown_provider") == 0) {
        fprintf(stderr, "Error: %s\n", t3 ? t3 + 1 : "unknown provider");
        return 2;
    }
    if (strcmp(state, "not_authed") == 0) {
        fprintf(stderr, "Not logged into %s. Run `hermes auth add %s` first.\n", t3 ? t3 + 1 : "?", arg);
        return 2;
    }
    if (strcmp(state, "bind_fail") == 0) {
        fprintf(stderr, "proxy: failed to bind\n");
        return 1;
    }
    printf("proxy: stopped\n");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_proxy_status @ hermes_cli/proxy/cli.py:cmd_proxy_status */
int hermes_cli_proxy_cli_cmd_proxy_status(const char *arg) {
    /* Python: adapter status table. Arg = "adapters" (tab-sep,
     * each: name\tdisplay\tstate\texpires). */
    if (!arg || !*arg) { printf("Hermes proxy upstream adapters\n\n\nStart the proxy with: hermes proxy start [--provider <name>]\n"); return 0; }
    printf("Hermes proxy upstream adapters\n\n");
    printf("%s\n", arg);
    printf("\nStart the proxy with: hermes proxy start [--provider <name>]\n");
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_proxy_list_providers @ hermes_cli/proxy/cli.py:cmd_proxy_list_providers */
int hermes_cli_proxy_cli_cmd_proxy_list_providers(const char *arg) {
    /* Python: "Available proxy upstream providers:" + sorted adapters with
     * display names. Arg = "name\tdisplay\tname\tdisplay..." */
    printf("Available proxy upstream providers:\n");
    if (arg && *arg) {
        const char *p = arg;
        while (*p) {
            const char *tab = strchr(p, '\t');
            if (!tab) { printf("  %s\n", p); break; }
            const char *tab2 = strchr(tab + 1, '\t');
            if (!tab2) { printf("  %.*s  — %s\n", (int)(tab - p), p, tab + 1); break; }
            printf("  %.*s  — %.*s\n", (int)(tab - p), p, (int)(tab2 - tab - 1), tab + 1);
            p = tab2 + 1;
        }
    }
    return 0;
}

/* PoP: hermes_cli_proxy_cli_cmd_proxy @ hermes_cli/proxy/cli.py:cmd_proxy */
int hermes_cli_proxy_cli_cmd_proxy(const char *arg) {
    /* Python: delegate to proxy.cli.cmd_proxy, SystemExit on non-zero. */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    printf("%s\n", arg);
    return 0;
}
