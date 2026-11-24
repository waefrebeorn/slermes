#ifndef NOC_W17_WEBSERVER_4C1444_H
#define NOC_W17_WEBSERVER_4C1444_H

#include "hermes_json.h"

/* C port of selected pure helpers in hermes_cli/web_server_gateway.py
 * (gate w17_webserver_4c1444). Returned char* values (where any) are malloc'd;
 * json_t* returns are owned by the caller. */

/* _windows_build_number(version, platform_label): extract the NT build from
 * stdlib platform strings. Returns true and sets *out_build on a match. */
bool w17_webserver_4c1444_windows_build_number(const char *version, const char *platform_label,
                                               long long *out_build);

/* _display_system_platform(*, system, release, version, platform_label). */
json_t *w17_webserver_4c1444_display_system_platform(const char *system, const char *release,
                                                     const char *version, const char *platform_label);

/* _named_profile_from_action(subcommand) -> str or NULL. */
char *w17_webserver_4c1444_named_profile_from_action(json_t *subcommand);

/* _is_host_gateway_spawn(subcommand) -> bool. */
bool w17_webserver_4c1444_is_host_gateway_spawn(json_t *subcommand);

/* _split_text_for_speak_stream(text, cap) -> JSON array of str.
 * Sentence boundaries and cap slicing are code-point (UTF-8 lead-byte) aware. */
json_t *w17_webserver_4c1444_split_text_for_speak_stream(const char *text, long long cap);

/* _strip_session_list_rows(sessions) -> JSON array (deep copy with the two
 * heavy fields popped). */
json_t *w17_webserver_4c1444_strip_session_list_rows(json_t *sessions);

#endif
