#ifndef NOC_AUXKEYROT_H
#define NOC_AUXKEYROT_H
/*
 * Public C API for the port of agent/auxiliary_key_rotation.py (gate auxkeyrot).
 * All exported symbols are module-prefixed to avoid collisions.
 */
/*
 * Decision core of rotate_runtime_main_api_key.
 *
 * The live function reads a shared dict from a ContextVar and mutates it
 * in place. The C port exposes that decision: given the current api_key,
 * the old key and the replacement, it reproduces the guard
 *   isinstance(runtime, dict) and runtime.get("api_key") == old
 * and the assignment runtime["api_key"] = _normalize_api_key(new).
 * ContextVar indirection is a seam: the caller supplies runtime contents;
 * runtime_is_dict models the isinstance() guard.
 *
 * old/new/current are NUL-terminated strings or NULL (= Python None).
 * In the tested domain `new` is never a callable, so _normalize_api_key
 * maps a str to str.strip() and any non-str to "".
 *
 * Returns a newly malloc'd NUL-terminated api_key string (caller frees),
 * or NULL for Python None. Sets *rotated to 1 iff the swap happened.
 * When runtime_is_dict is 0 there is no published runtime dict and therefore
 * no api_key to report: NULL is returned.
 */
char *auxkeyrot_rotate_runtime_main_api_key(const char *old, int old_is_null,
        const char *new_key, int new_is_null,
        const char *current, int current_is_null,
        int runtime_is_dict, int *rotated);

#endif /* NOC_AUXKEYROT_H */
