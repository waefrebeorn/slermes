/* Slermes C11 port of gateway/dead_targets.py
 *
 * Persistent registry of delivery targets confirmed unreachable (deleted group,
 * kicked/blocked bot, deactivated user). Lets the delivery layer short-circuit a
 * target already proven dead, while staying self-healing: a successful send
 * clears the flag. Keyed on "platform:chat_id". Best-effort persistence to a
 * small JSON file under HERMES_HOME; a corrupt/unwritable file degrades to an
 * in-memory-only registry rather than breaking the delivery path.
 *
 * PoP: exact port. Semantic source of truth = gateway/dead_targets.py.
 */
#ifndef SLERMES_DEAD_TARGETS_H
#define SLERMES_DEAD_TARGETS_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque thread-safe, persistent set of confirmed-dead delivery targets. */
typedef struct dead_target_registry dead_target_registry_t;

/* Construct a registry. `path` overrides the store location (for tests); pass
 * NULL to use <hermes_home>/gateway/dead_targets.json. Loads existing state. */
dead_target_registry_t *dead_target_registry_create(const char *path);

/* Free a registry (does not touch the on-disk file). */
void dead_target_registry_free(dead_target_registry_t *r);

/* True when `error_kind` denotes a permanent whole-chat death
 * ("forbidden" or "not_found"). Static — no registry needed. */
bool dead_target_is_dead_error_kind(const char *error_kind);

/* Best-effort atomic persist of the current dead set to disk (internal). */
void dead_target_flush_locked(dead_target_registry_t *r);

/* True when (platform, chat_id) is currently flagged dead. */
bool dead_target_is_dead(dead_target_registry_t *r, const char *platform,
                         const char *chat_id);

/* Record a target as confirmed-dead (reason truncated to 200 chars).
 * Returns true if newly added (false if already present or chat_id empty). */
bool dead_target_mark_dead(dead_target_registry_t *r, const char *platform,
                           const char *chat_id, const char *reason);

/* Remove a target's dead flag (self-healing). Returns true if it was set. */
bool dead_target_clear(dead_target_registry_t *r, const char *platform,
                       const char *chat_id);

/* Snapshot of the current dead set as a JSON object (caller frees). */
json_t *dead_target_all_dead(dead_target_registry_t *r);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_DEAD_TARGETS_H */
