#ifndef NOC_W6_TURNACICAL_C014D7_H
#define NOC_W6_TURNACICAL_C014D7_H
#include <stddef.h>

/* C home for upstream Hermes agent/turn_api_call.py (gate w6_turnapical_c014d7).
 * The streaming decision is a pure branch over the agent's observable state;
 * all provider/profile lookups are injected by the caller as explicit seams. */

/* PoP: w6_turnapical_c014d7_should_stream @ agent/turn_api_call.py:_should_stream
 * supports_streaming mirrors getattr(profile, "supports_streaming", None):
 *   -1 => None, 0 => False, 1 => True.
 * Returns 1 for True, 0 for False. */
int w6_turnapical_c014d7_should_stream(
    int disable_streaming, const char *base_url, const char *provider,
    int supports_streaming, int external_process_provider,
    int has_stream_consumers, int client_is_mock);

#endif /* NOC_W6_TURNACICAL_C014D7_H */
