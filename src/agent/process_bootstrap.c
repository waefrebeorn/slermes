/*
 * process_bootstrap.c — Process-level bootstrap helpers (P80).
 *
 * Port of Python agent/process_bootstrap.py (167 lines, 13 defs).
 * _get_proxy_from_env and _get_proxy_for_base_url are in proxy_utils.c.
 * _OpenAIProxy and _SafeWriter classes are Python-only (NA_SDK).
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* ================================================================
 *  Lazy OpenAI SDK import — C returns NULL (uses own http client)
 * ================================================================ */

/* PoP: load_openai_cls @ agent/process_bootstrap.py:_load_openai_cls */
/* PoP: cli_agent_auxiliary_client__load_openai_cls @ agent/auxiliary_client.py:_load_openai_cls */
const void *load_openai_cls(void) {
    /*
     * C's OpenAI-compatible client is built into the provider system
     * via libhttp. The OpenAI Python SDK class is not instantiable
     * from C. Callers that need a full OpenAI SDK client should use
     * the Python process directly.
     */
    hermes_log(LOG_DEBUG, "process_bootstrap",
               "load_openai_cls: OpenAI SDK not available in C, returning NULL");
    return NULL;
}

/* ================================================================
 *  Crash-resistant stdio — C stdio doesn't raise OSError
 * ================================================================ */

/* PoP: install_safe_stdio @ agent/process_bootstrap.py:_install_safe_stdio */
void install_safe_stdio(void) {
    /*
     * C's stdout/stderr are raw file descriptors that do not raise
     * OSError/ValueError when the pipe is broken — write() returns
     * -1 with EPIPE instead, which the pre-existing signal handler
     * (SIGPIPE = SIG_IGN) handles. This is a no-op in C.
     *
     * We set SIGPIPE to SIG_IGN if not already set, so that
     * broken pipe conditions return errors rather than killing
     * the process.
     */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);
    hermes_log(LOG_DEBUG, "process_bootstrap",
               "install_safe_stdio: SIGPIPE set to SIG_IGN (C stdio is inherently safe)");
}
