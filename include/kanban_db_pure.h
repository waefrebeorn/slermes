#ifndef HERMES_KANBAN_DB_PURE_H
#define HERMES_KANBAN_DB_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/kanban_db.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

char *kanban_normalize_reasoning_effort(const char *effort);
int   kanban_resolve_claim_ttl_seconds(int ttl_seconds);
int   kanban_resolve_crash_grace_seconds(const char *env);
int   kanban_resolve_rate_limit_cooldown_seconds(const char *env);
char *kanban_relative_age(long ts, long now);
char *kanban_normalize_board_slug(const char *slug);
char *kanban_default_board_display_name(const char *slug);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_KANBAN_DB_PURE_H */
