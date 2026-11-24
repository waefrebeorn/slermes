#ifndef NOC_W19_USAGE_8EC762_H
#define NOC_W19_USAGE_8EC762_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_usage_8ec762_delta_kwargs @ persistence/sqlite/conversation/usage.py:_delta_kwargs
 *
 * The ``update_token_counts`` keyword mapping for one queued usage delta.  The live
 * Python reads duck-typed attributes (``delta.route`` / ``delta.tokens`` / ...); here
 * the delta arrives as a JSON object with the same field names, ``route`` and
 * ``tokens`` being nested objects (``route`` may be null; a null route contributes
 * null billing/model fields).  Values are copied verbatim, exactly like the Python
 * dict literal does.  Returns a new JSON object (caller frees). */
struct json_t *w19_usage_8ec762_delta_kwargs(const struct json_t *delta);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_USAGE_8EC762_H */
