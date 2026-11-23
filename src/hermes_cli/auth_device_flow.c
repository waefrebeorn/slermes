/*
 * auth_device_flow.c - C home for upstream Hermes hermes_cli/auth_device_flow.py.
 * PoP: w15_authdevice_1acd9a__names_console_browser @ hermes_cli/auth_device_flow.py:_names_console_browser
 * PoP: w15_authdevice_1acd9a__nous_device_auth_timeout_message @ hermes_cli/auth_device_flow.py:_nous_device_auth_timeout_message
 *
 * The two environment-independent helpers are ported faithfully. Browser
 * resolution, TLS, HTTP polling and loopback-server helpers bind live sockets /
 * webbrowser state and stay with the live Python owner.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_authdevice_1acd9a.h"

static const char *const CONSOLE_NAMES[] = {
    "w3m", "lynx", "links", "links2", "elinks", "www-browser", "browsh", NULL
};

static int is_console_name(const char *name) {
    for (int i = 0; CONSOLE_NAMES[i]; i++)
        if (!strcmp(name, CONSOLE_NAMES[i])) return 1;
    return 0;
}

int w15_authdevice_1acd9a_names_console_browser(const char *value) {
    if (!value) return 0;
    const char *p = value;
    while (*p && isspace((unsigned char)*p)) p++;       /* value.strip() */
    if (!*p) return 0;                                   /* empty after strip */
    const char *q = p;
    while (*q && !isspace((unsigned char)*q)) q++;       /* .split()[0] */
    const char *base = p;                                /* os.path.basename: last / or \ */
    for (const char *c = p; c < q; c++)
        if (*c == '/' || *c == '\\') base = c + 1;
    char buf[256];
    size_t n = (size_t)(q - base);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)base[i]);
    buf[n] = '\0';
    return is_console_name(buf);
}

struct json_t *w15_authdevice_1acd9a_nous_device_auth_timeout_message(const char *portal_base_url) {
    const char *portal = (portal_base_url && *portal_base_url)
        ? portal_base_url : "https://portal.nousresearch.com";
    size_t plen = strlen(portal);
    while (plen && portal[plen - 1] == '/') plen--;      /* .rstrip("/") */
    size_t cap = 512 + plen + 32;
    char *b = (char *)malloc(cap);
    if (!b) return json_string("");
    int k = snprintf(b, cap,
        "Timed out waiting for device authorization.\n"
        "  Portal sign-in is required before the device code can be approved.\n"
        "  If the browser showed a CAPTCHA / 'You did not pass CAPTCHA' error,\n"
        "  finish signing in at the Portal in a normal browser tab, then retry:\n"
        "    hermes portal\n"
        "  Portal login: %.*s/login",
        (int)plen, portal);
    json_t *r = json_string(k > 0 && (size_t)k < cap ? b : "");
    free(b);
    return r;
}
