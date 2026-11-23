/*
 * loop_noise.c -- faithful C11 port of upstream Hermes tui_gateway/loop_noise.py.
 *
 * PoP: w9_loopnoise_e25da7_is_benign_teardown @ tui_gateway/loop_noise.py:_is_benign_teardown
 *
 * The asyncio exception-handler installation is not portable; the predicate that
 * decides whether an event-loop error is a benign peer hangup is, and is ported
 * verbatim (exception type AND the "_call_connection_lost" marker on callback/handle).
 */
#include "noc_w9_loopnoise_e25da7.h"
#include <string.h>

static int benign_exception(const char *kind) {
    return kind && (!strcmp(kind, "ConnectionResetError") ||
                    !strcmp(kind, "ConnectionAbortedError") ||
                    !strcmp(kind, "BrokenPipeError"));
}

int w9_loopnoise_e25da7_is_benign_teardown(const char *exc_kind,
                                           const char *callback_repr,
                                           const char *handle_repr) {
    if (!benign_exception(exc_kind)) return 0;
    if (callback_repr && strstr(callback_repr, "_call_connection_lost")) return 1;
    if (handle_repr && strstr(handle_repr, "_call_connection_lost")) return 1;
    return 0;
}
