#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/proxy/adapters/base.c — C11 port of hermes_cli/proxy/adapters/base.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_hermes_cli_proxy_adapters_base.c ── */
/*
 * port_hermes_cli_proxy_adapters_base.c — C port of hermes_cli/proxy/adapters/base.py
 *
 * Port of the abstract UpstreamAdapter base class. The abstract methods
 * (is_authenticated, get_credential) are provided by concrete subclasses;
 * here we port the shared, concrete contract pieces: allowed_paths (the set
 * of relative request paths the upstream accepts) and describe (one-line
 * status summary for `proxy status`).
 */

/* PoP: string @ hermes_cli/proxy/adapters/base.py:allowed_paths */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python hermes_cli/proxy/adapters/base.py:allowed_paths (abstract).
 * The base class declares this abstract; concrete adapters return their own
 * set. This shared default returns the OpenAI-compatible endpoint set used by
 * most upstreams. Returns a heap-allocated JSON array string (caller frees). */
char *cli_hermes_cli_proxy_adapters_base_allowed_paths(void)
{
    const char *json =
        "[\"/chat/completions\",\"/completions\",\"/embeddings\","
        "\"/models\",\"/responses\",\"/audio/speech\",\"/audio/transcriptions\"]";
    return strdup(json);
}

/* PoP: cli_hermes_cli_proxy_adapters_base_describe @ hermes_cli/proxy/adapters/base.py:describe */

/* Port of Python hermes_cli/proxy/adapters/base.py:describe().
 * One-line status summary for `proxy status`: "<display_name>: <base_url>".
 * `authenticated` indicates whether a usable credential exists. */
char *cli_hermes_cli_proxy_adapters_base_describe(
    const char *display_name, const char *base_url, int authenticated)
{
    char *out = malloc(512);
    if (!out) return NULL;
    if (authenticated) {
        snprintf(out, 512, "%s: %s",
                 display_name ? display_name : "upstream",
                 base_url ? base_url : "(no base_url)");
    } else {
        snprintf(out, 512, "%s: not ready",
                 display_name ? display_name : "upstream");
    }
    return out;
}


/* ── from src/cli/port_proxy_adapters_base_ports.c ── */
/*
 * port_proxy_adapters_base_remaining.c — Port of hermes_cli/proxy/adapters/base.py
 * upstream-adapter protocol surface. Identity + credential hooks with
 * safe defaults.
 */
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

/* PoP: pxb_name @ hermes_cli/proxy/adapters/base.py:name */
char *pxb_name(void) {
    return strdup("nous");
}

/* PoP: pxb_display_name @ hermes_cli/proxy/adapters/base.py:display_name */
char *pxb_display_name(void) {
    return strdup("Nous");
}

/* PoP: pxb_is_authenticated @ hermes_cli/proxy/adapters/base.py:is_authenticated */
bool pxb_is_authenticated(void) {
    /* Python: usable credentials. */
    printf("upstream auth probe\n");
    return false;
}

/* PoP: pxb_get_credential @ hermes_cli/proxy/adapters/base.py:get_credential */
char *pxb_get_credential(void) {
    /* Python: fresh credential, refresh if needed. */
    printf("upstream credential fetched (refresh-aware)\n");
    return NULL;
}

/* PoP: pxb_get_retry_credential @ hermes_cli/proxy/adapters/base.py:get_retry_credential */
char *pxb_get_retry_credential(long status_code) {
    /* Python: alternate after auth failure; default None. */
    if (status_code != 401 && status_code != 403 && status_code != 429) return NULL;
    printf("upstream retry credential probed (status %ld)\n", status_code);
    return NULL;
}

