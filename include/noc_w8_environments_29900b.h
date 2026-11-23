#ifndef NOC_W8_ENVIRONMENTS_29900B_H
#define NOC_W8_ENVIRONMENTS_29900B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_environments_29900b_replace @ pm/environments_adopt.py:_replace
 * Explicit `sep` seam replaces os.pathsep. Resolves `part` and `old`
 * lexically; a part equal to old is replaced by `new`. Returns a malloc'd
 * string; caller frees. */
char *w8_environments_29900b_replace(const char *value, const char *old,
                                     const char *newp, const char *sep);

/* PoP: w8_environments_29900b_restart_needed_ex @ pm/environments_adopt.py:restart_needed
 * Explicit seam: the already-resolved (running, selected) pair is injected.
 * Returns a malloc'd reason string, or NULL (None) when they are the same
 * generation. Caller frees. */
char *w8_environments_29900b_restart_needed_ex(const char *running, const char *selected);

/* PoP: w8_environments_29900b_note_pending @ pm/environments_adopt.py:note_restart_pending
 * Stores `value` (or NULL for None) in the module-level _restart_pending slot
 * and returns a malloc'd copy of the stored reason, or NULL. Caller frees. */
char *w8_environments_29900b_note_pending(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_ENVIRONMENTS_29900B_H */
