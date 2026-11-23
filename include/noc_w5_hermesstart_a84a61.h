#ifndef NOC_W5_HERMESSTART_A84A61_H
#define NOC_W5_HERMESSTART_A84A61_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_startup_watchdog.py (gate w5_hermesstart_a84a61).
 * Only the pure env/config decision seams are ported; the thread, fire path and
 * dumps stay in Python. Env values are injected explicitly to avoid CRT setenv
 * semantics differing from Python os.environ. */

/* PoP: w5_hermesstart_a84a61_disabled @ hermes_startup_watchdog.py:startup_watchdog_disabled
 * env_value is the value of HERMES_STARTUP_WATCHDOG (NULL treated as ""). */
struct json_t *w5_hermesstart_a84a61_disabled(const char *env_value);

/* PoP: w5_hermesstart_a84a61_resolve_timeout @ hermes_startup_watchdog.py:resolve_startup_watchdog_timeout
 * raw is HERMES_STARTUP_WATCHDOG_TIMEOUT_S. Returns a JSON number. */
struct json_t *w5_hermesstart_a84a61_resolve_timeout(const char *raw);

/* PoP: w5_hermesstart_a84a61_dump_path @ hermes_startup_watchdog.py:get_startup_watchdog_dump_path
 * Returns "<home>/logs/gateway-startup-watchdog.log". */
struct json_t *w5_hermesstart_a84a61_dump_path(const char *home);

/* PoP: w5_hermesstart_a84a61_kick_extra @ hermes_startup_watchdog.py:StartupWatchdogHandle.kick
 * The pure max(0.0, float(extra_s)) coercion; invalid input yields 0.0. */
struct json_t *w5_hermesstart_a84a61_kick_extra(const char *extra_s);

/* PoP: w5_hermesstart_a84a61_lease_clamp @ hermes_startup_watchdog.py:StartupWatchdogHandle.lease
 * The pure float/min(_MAX_LEASE_S) gate; invalid or non-positive input is null. */
struct json_t *w5_hermesstart_a84a61_lease_clamp(const char *expected_s);

/* PoP: w5_hermesstart_a84a61_arm_timeout @ hermes_startup_watchdog.py:arm_startup_watchdog
 * timeout_s NULL means "not supplied"; disabled and env_raw are injected seams.
 * Returns the resolved timeout number, or JSON null when disarmed/invalid. */
struct json_t *w5_hermesstart_a84a61_arm_timeout(const char *timeout_s,
                                                 int disabled, const char *env_raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_HERMESSTART_A84A61_H */
