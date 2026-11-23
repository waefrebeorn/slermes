#ifndef NOC_W5_HERMESSTATE_A0163A_H
#define NOC_W5_HERMESSTATE_A0163A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_state_loop_guard.py (gate w5_hermesstate_a0163a).
 * The idempotent install mode decision and the config.yaml startup hook are
 * ported; the actual StoreHolder monkeypatch and warning/record state stay in
 * Python. current_mode is an injected seam for the module's _state['mode']. */

/* PoP: w5_hermesstate_a0163a_valid_mode @ hermes_state_loop_guard.py:install
 * Whether mode is one of ('warn', 'raise', 'record'). */
struct json_t *w5_hermesstate_a0163a_valid_mode(const char *mode);

/* PoP: w5_hermesstate_a0163a_install_mode @ hermes_state_loop_guard.py:install
 * Returns mode when valid, else the injected current mode (or null). */
struct json_t *w5_hermesstate_a0163a_install_mode(const char *mode, const char *current_mode);

/* PoP: w5_hermesstate_a0163a_install_from_config @ hermes_state_loop_guard.py:install_from_config
 * cfg is the raw config mapping; current_mode is injected. Returns the mode
 * that would be installed, or null for unset/empty/off/false/none/invalid. */
struct json_t *w5_hermesstate_a0163a_install_from_config(const struct json_t *cfg,
                                                         const char *current_mode);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_HERMESSTATE_A0163A_H */
