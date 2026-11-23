#ifndef NOC_W14_RUNTIMSES_86879C_H
#define NOC_W14_RUNTIMSES_86879C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_runtimeses_86879c_apply_row_annotations @ agent/runtime_session_store.py:apply_row_annotations
 * Mirrors the owner's in-place row stamps onto message dicts; a null annotation
 * value pops the key. Returns the mutated messages array. */
struct json_t *w14_runtimeses_86879c_apply_row_annotations(const struct json_t *messages,
        const struct json_t *annotations);

/* PoP: w14_runtimeses_86879c_session_number @ agent/runtime_session_store.py:RuntimeSessionStore._session_number
 * max(zero, cast(row[column] or zero)); cast_kind is "int" or "float".
 * A failed cast returns zero (Python's except path). */
struct json_t *w14_runtimeses_86879c_session_number(const struct json_t *row,
        const char *column, const char *cast_kind, double zero);

/* PoP: w14_runtimeses_86879c_compression_cooldown @ agent/runtime_session_store.py:RuntimeSessionStore.get_compression_failure_cooldown
 * `now` is an injected seam for time.time(). Returns JSON null when no live
 * cooldown, else {cooldown_until, remaining_seconds, error}. */
struct json_t *w14_runtimeses_86879c_compression_cooldown(const struct json_t *row, double now);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_RUNTIMSES_86879C_H */
