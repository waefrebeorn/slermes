#ifndef NOC_W19_ERRORS_C3B6AF_H
#define NOC_W19_ERRORS_C3B6AF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_errors_c3b6af_cause @ persistence/core/errors.py:<class>.cause
 * The store's error-classification bucket a taxonomy class answers for. */
const char *w19_errors_c3b6af_cause(const char *cls);

/* PoP: w19_errors_c3b6af_is_kind @ persistence/core/errors.py:<module>
 * 1 when `cls` is (a subclass of) `base` in the live taxonomy. */
int w19_errors_c3b6af_is_kind(const char *cls, const char *base);

/* PoP: w19_errors_c3b6af_in_group @ persistence/core/errors.py:STORE_FAILURES
 * Group membership: "store_failures", "mutation_guards", "persistence_error",
 * "transient", "permanent", "authority", "conflict", "not_found", "corrupt",
 * "replaced", or "cause" (a bucket of PERSISTENCE_ERROR_CAUSES). */
int w19_errors_c3b6af_in_group(const char *cls, const char *group);

/* PoP: w19_errors_c3b6af_message @ persistence/core/errors.py:<class>.__init__
 * The message text `str(exc)` answers for a class built from `args` (an object
 * naming the constructor parameters). Malloc'd; NULL when the class needs an
 * argument the table cannot synthesize (e.g. an explicit message). */
char *w19_errors_c3b6af_message(const char *cls, json_t *args);

/* PoP: w19_errors_c3b6af_one_line @ persistence/core/errors.py:_one_line
 * Whitespace collapsed, non-printables dropped, bounded by `limit` codepoints.
 * None for a null value or an empty result. */
json_t *w19_errors_c3b6af_one_line(json_t *value, long long limit);

/* PoP: w19_errors_c3b6af_busy_refusal @ persistence/core/errors.py:busy_refusal
 * {"reason":"session_busy","field":null|"compression"} for one of
 * MUTATION_GUARD_REFUSALS. */
json_t *w19_errors_c3b6af_busy_refusal(const char *exc);

/* PoP: w19_errors_c3b6af_rewind_text @ persistence/core/errors.py:_REWIND_REFUSAL_TEXT
 * The usual text of a rewind refusal reason, or NULL for an unknown reason. */
const char *w19_errors_c3b6af_rewind_text(const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ERRORS_C3B6AF_H */
