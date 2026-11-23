#ifndef NOC_W7_TURNEXIT_39298B_H
#define NOC_W7_TURNEXIT_39298B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_turnexit_39298b_turn_exit_code @ hermes_cli/turn_exit.py:turn_exit_code
 * Kanban codes are the real constants from hermes_cli/kanban_db.py:
 * RATE_LIMIT=75, TERMINAL_PROVIDER=78. */
int w7_turnexit_39298b_turn_exit_code(const struct json_t *result,
                                      int kanban_worker,
                                      int credentials_rate_limited,
                                      int credentials_terminal);

/* PoP: w7_turnexit_39298b_reasons @ hermes_cli/turn_exit.py:TRANSIENT_PROVIDER_REASONS / TERMINAL_PROVIDER_REASONS
 * which = "transient" or "terminal"; returns the sorted frozenset members. */
struct json_t *w7_turnexit_39298b_reasons(const char *which);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_TURNEXIT_39298B_H */
