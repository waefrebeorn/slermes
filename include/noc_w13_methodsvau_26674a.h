#ifndef NOC_W13_METHODSVAU_26674A_H
#define NOC_W13_METHODSVAU_26674A_H

struct json_t;
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: methodsvau_remove @ tui_gateway/methods_vault.py:vault.remove
 * Guard + result: {"code":5095,"error":"id is required"} or {"removed": <seam>}.
 * `removed` is a JSON bool seam standing in for get_vault_store().remove_item(). */
struct json_t *methodsvau_remove(const struct json_t *params, const struct json_t *removed);

/* PoP: methodsvau_lock @ tui_gateway/methods_vault.py:vault.lock
 * Always {"locked": true}. */
struct json_t *methodsvau_lock(void);

/* PoP: methodsvau_add @ tui_gateway/methods_vault.py:vault.add
 * `add_result` seam: {"id": <v>} on success or {"error": <raw msg>} on failure.
 * The failure message is scrubbed exactly like agent.vault_store.scrub_secret_from_text. */
struct json_t *methodsvau_add(const struct json_t *params, const struct json_t *add_result);

/* PoP: methodsvau_unlock @ tui_gateway/methods_vault.py:vault.unlock
 * `backends` = array of {"name","needs_unlock"}; `exc_msg` = unlock failure text or null. */
struct json_t *methodsvau_unlock(const struct json_t *params, const struct json_t *backends,
                                 const struct json_t *exc_msg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_METHODSVAU_26674A_H */
