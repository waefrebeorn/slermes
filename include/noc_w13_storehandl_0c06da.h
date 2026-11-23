#ifndef NOC_W13_STOREHANDL_0C06DA_H
#define NOC_W13_STOREHANDL_0C06DA_H

struct json_t;
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: storehandl_is_handle @ tui_gateway/store_handles.py:_is_handle
 * JSON model: db is null or an object with "is_handle" (bool). Returns bool. */
struct json_t *storehandl_is_handle(const struct json_t *db);

/* PoP: storehandl_launch_persistence @ tui_gateway/store_handles.py:_launch_persistence
 * Seam: launch_is_handle (bool) -> {"handle":"launch"} or null. */
struct json_t *storehandl_launch_persistence(const struct json_t *launch_is_handle);

/* PoP: storehandl_persistence_beside @ tui_gateway/store_handles.py:_persistence_beside
 * db model object fields: is_handle, is_launch, scope. Seam: launch_is_handle.
 * Returns {"handle":"launch"} / {"handle":"own","scope":<s>} / null. */
struct json_t *storehandl_persistence_beside(const struct json_t *db,
                                             const struct json_t *launch_is_handle);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_STOREHANDL_0C06DA_H */
