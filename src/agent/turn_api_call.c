/*
 * turn_api_call.c — C home for upstream Hermes agent/turn_api_call.py.
 * Ported behavior: the _should_stream() decision tree. Request execution,
 * interrupt handling and the Nous rate guard stay in Python (all I/O +
 * in-place agent mutation).
 * PoP: w6_turnapical_c014d7_should_stream @ agent/turn_api_call.py:_should_stream
 */
#include "noc_w6_turnapical_c014d7.h"
#include <ctype.h>
#include <string.h>

/* str(base_url or "").lower().startswith(prefix) for an ASCII prefix. */
static int starts_with_folded(const char *s, const char *prefix) {
    size_t i;
    if (!s) return 0;
    for (i = 0; prefix[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!c || tolower(c) != tolower((unsigned char)prefix[i])) return 0;
    }
    return 1;
}

int w6_turnapical_c014d7_should_stream(
    int disable_streaming, const char *base_url, const char *provider,
    int supports_streaming, int external_process_provider,
    int has_stream_consumers, int client_is_mock)
{
    /* if getattr(agent, "_disable_streaming", False): return False */
    if (disable_streaming) return 0;

    /* _base = str(agent.base_url or "").lower(); ACP transports never stream */
    if (starts_with_folded(base_url, "acp://") ||
        starts_with_folded(base_url, "acp+tcp://")) return 0;

    /* profile.supports_streaming is False -> explicit opt-out */
    if (supports_streaming == 0) return 0;

    /* process providers must opt in (streaming is True) */
    if (external_process_provider && supports_streaming != 1) return 0;

    if (!has_stream_consumers) {
        if (provider && !strcmp(provider, "moa")) return 0;
        if (client_is_mock) return 0;
    }
    return 1;
}
