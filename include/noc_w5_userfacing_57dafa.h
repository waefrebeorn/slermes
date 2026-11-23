#ifndef NOC_W5_USERFACING_57DAFA_H
#define NOC_W5_USERFACING_57DAFA_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_userfacing_57dafa_copies @ pm/user_facing.py:copies
 * Environment seams are injected: `applies`, per-name sealed dirs and per-name
 * writable copies. Returns the deduplicated, normcase-keyed replacement list. */
struct json_t *w5_userfacing_57dafa_copies(const struct json_t *dirs, int applies,
                                           const struct json_t *sealed_map,
                                           const struct json_t *copy_map);

/* PoP: w5_userfacing_57dafa_user_facing_path @ pm/user_facing.py:user_facing_path
 * The store dirs, `copies(store)` result, `copies(_python_dirs())` result and the
 * python-stub classification are injected seams; the reorder/dedup logic is the port. */
char *w5_userfacing_57dafa_user_facing_path(const char *path,
                                            const struct json_t *store,
                                            const struct json_t *ours,
                                            const struct json_t *python_copies,
                                            const struct json_t *stub_flags);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_USERFACING_57DAFA_H */
