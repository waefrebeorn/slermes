/*
 * turn_api_error.c — real C11 port of upstream Hermes agent/turn_api_error.py.
 *
 * PoP: turnapierr_is_local_validation_error @ agent/turn_api_error.py:_is_local_validation_error
 *
 * The upstream module is an orchestration handler whose only deterministic,
 * side-effect-free unit is _is_local_validation_error. Its collaborators
 * (agent.error_classifier, agent.turn_recovery, agent.route_verdicts,
 * agent.api_error_summary.is_provider_stream_parse_error) are injected by the
 * oracle; the C port mirrors the predicate exactly, including the exception
 * subclass exclusions and the "'NoneType' object is not iterable" TypeError
 * exception that makes an upstream shape mismatch retryable.
 */
#include "noc_w7_turnapierr_7027a9.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ASCII lower-casing of the exception message, matching Python str.lower()
 * for the ASCII markers this predicate looks for. */
static char *ascii_lower(const char *s) {
    size_t n = s ? strlen(s) : 0, i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

int turnapierr_is_local_validation_error(const char *exc_type, const char *message, int stream_parse) {
    int is_value_or_type, excluded, is_type_error;
    char *text;
    int retryable_shape;

    if (!exc_type) return 0;

    /* isinstance(api_error, (ValueError, TypeError)): the ValueError family
     * includes UnicodeEncodeError, json.JSONDecodeError and ssl.SSLError;
     * TypeError is its own arm. Everything else is not a local validation
     * error. */
    is_value_or_type = !strcmp(exc_type, "ValueError")
                    || !strcmp(exc_type, "TypeError")
                    || !strcmp(exc_type, "UnicodeEncodeError")
                    || !strcmp(exc_type, "JSONDecodeError")
                    || !strcmp(exc_type, "SSLError");
    if (!is_value_or_type) return 0;

    /* isinstance(api_error, (UnicodeEncodeError, json.JSONDecodeError,
     * ssl.SSLError)): never a local bug. */
    excluded = !strcmp(exc_type, "UnicodeEncodeError")
            || !strcmp(exc_type, "JSONDecodeError")
            || !strcmp(exc_type, "SSLError");
    if (excluded) return 0;

    /* is_provider_stream_parse_error(api_error) is injected. */
    if (stream_parse) return 0;

    /* _text = str(api_error).lower() */
    text = ascii_lower(message);
    if (!text) return 0;
    is_type_error = !strcmp(exc_type, "TypeError");
    retryable_shape = is_type_error
                   && strstr(text, "nonetype") != NULL
                   && strstr(text, "not iterable") != NULL;
    free(text);

    /* return not (isinstance(api_error, TypeError)
     *             and "nonetype" in _text and "not iterable" in _text) */
    return retryable_shape ? 0 : 1;
}
