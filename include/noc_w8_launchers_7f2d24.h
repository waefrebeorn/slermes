#ifndef NOC_W8_LAUNCHERS_7F2D24_H
#define NOC_W8_LAUNCHERS_7F2D24_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful C11 port of the pure string/constant helpers in
 * hermes_cli/_launchers.py. */

/* PoP: w8_launchers_7f2d24_pin_default_home_flag @ hermes_cli/_launchers.py:PIN_DEFAULT_HOME_FLAG */
const char *w8_launchers_7f2d24_pin_default_home_flag(void);

/* PoP: w8_launchers_7f2d24_windows_bin_launchers @ hermes_cli/_launchers.py:WINDOWS_BIN_LAUNCHERS */
struct json_t *w8_launchers_7f2d24_windows_bin_launchers(void);

/* PoP: w8_launchers_7f2d24_entry_points @ hermes_cli/_launchers.py:ENTRY_POINTS */
struct json_t *w8_launchers_7f2d24_entry_points(void);

/* PoP: w8_launchers_7f2d24_inline_string_literal @ hermes_cli/_launchers.py:_inline_string_literal */
struct json_t *w8_launchers_7f2d24_inline_string_literal(const char *value);

/* PoP: w8_launchers_7f2d24_first_token @ hermes_cli/_launchers.py:_first_token
 * Returns str(Path(token)) as seen on this Windows host (separators normalised). */
struct json_t *w8_launchers_7f2d24_first_token(const char *line);

/* PoP: w8_launchers_7f2d24_merge_user_path @ hermes_cli/_launchers.py:_merge_user_path */
struct json_t *w8_launchers_7f2d24_merge_user_path(const char *existing, const char *entry);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_LAUNCHERS_7F2D24_H */
