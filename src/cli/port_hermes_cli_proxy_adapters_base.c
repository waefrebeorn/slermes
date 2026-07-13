/*
 * port_hermes_cli_proxy_adapters_base.c — C port of hermes_cli/proxy/adapters/base.py
 *
 * Port of the abstract UpstreamAdapter base class. The abstract methods
 * (is_authenticated, get_credential) are provided by concrete subclasses;
 * here we port the shared, concrete contract pieces: allowed_paths (the set
 * of relative request paths the upstream accepts) and describe (one-line
 * status summary for `proxy status`).
 */

/* PoP: cli_hermes_cli_proxy_adapters_base_allowed_paths @ hermes_cli/proxy/adapters/base.py:allowed_paths */

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
