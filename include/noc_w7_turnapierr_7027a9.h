#ifndef NOC_W7_TURNAPIERR_7027A9_H
#define NOC_W7_TURNAPIERR_7027A9_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: turnapierr_is_local_validation_error @ agent/turn_api_error.py:_is_local_validation_error
 *
 * Faithful C11 port of the module's only pure predicate. `exc_type` is the
 * exception class name ("ValueError", "TypeError", "UnicodeEncodeError",
 * "JSONDecodeError", "SSLError", or any unrelated name); `message` is str(exc);
 * `stream_parse` is the result of the injected
 * agent.api_error_summary.is_provider_stream_parse_error collaborator.
 * Returns 1 when the exception is a "local validation error" (a plain
 * ValueError/TypeError that is not one of the retryable special cases),
 * else 0. */
int turnapierr_is_local_validation_error(const char *exc_type, const char *message, int stream_parse);

#ifdef __cplusplus
}
#endif
#endif
