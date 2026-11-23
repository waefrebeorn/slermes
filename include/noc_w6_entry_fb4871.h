#ifndef NOC_W6_ENTRY_FB4871_H
#define NOC_W6_ENTRY_FB4871_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_entry_fb4871_grace_default @ tui_gateway/entry.py:_DEFAULT_SHUTDOWN_GRACE_S */
double w6_entry_fb4871_grace_default(void);

/* PoP: w6_entry_fb4871_shutdown_grace @ tui_gateway/entry.py:_shutdown_grace_seconds
 * env_value is the injected HERMES_TUI_GATEWAY_SHUTDOWN_GRACE_S string
 * (NULL/"" means absent/empty -> default). Mirrors env_float(s, 1.0) then
 * `value if value > 0 else default`. */
double w6_entry_fb4871_shutdown_grace(const char *env_value);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_ENTRY_FB4871_H */
