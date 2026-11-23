#ifndef NOC_W15_BROWSERSES_71B99F_H
#define NOC_W15_BROWSERSES_71B99F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Each function mirrors the named live function with `browser_section()`
 * injected as `section` (the live module reads config at call time). */

/* PoP: w15_browserses_71b99f_command_timeout @ tools/browser_session_cloud.py:command_timeout */
long long w15_browserses_71b99f_command_timeout(const struct json_t *section);

/* PoP: w15_browserses_71b99f_inactivity_timeout @ tools/browser_session_cloud.py:inactivity_timeout */
long long w15_browserses_71b99f_inactivity_timeout(const struct json_t *section, long long def);

/* PoP: w15_browserses_71b99f_chromium_args @ tools/browser_session_cloud.py:chromium_args
 * Returns a NEW JSON array of stripped, non-empty flag strings. Caller frees. */
struct json_t *w15_browserses_71b99f_chromium_args(const struct json_t *section);

/* PoP: w15_browserses_71b99f_executable_path @ tools/browser_session_cloud.py:executable_path
 * Returns a malloc'd string (caller frees). */
char *w15_browserses_71b99f_executable_path(const struct json_t *section);

/* PoP: w15_browserses_71b99f_browser_engine @ tools/browser_session_cloud.py:get_browser_engine
 * Returns a malloc'd string (caller frees). */
char *w15_browserses_71b99f_browser_engine(const struct json_t *section);

/* PoP: w15_browserses_71b99f_is_headed @ tools/browser_session_cloud.py:is_headed_mode */
int w15_browserses_71b99f_is_headed(const struct json_t *section);

/* PoP: w15_browserses_71b99f_allow_private_urls @ tools/browser_session_cloud.py:allow_private_urls */
int w15_browserses_71b99f_allow_private_urls(const struct json_t *section);

/* PoP: w15_browserses_71b99f_auto_local_private @ tools/browser_session_cloud.py:auto_local_for_private_urls */
int w15_browserses_71b99f_auto_local_private(const struct json_t *section);

/* PoP: w15_browserses_71b99f_use_real_profile @ tools/browser_session_cloud.py:use_real_profile */
int w15_browserses_71b99f_use_real_profile(const struct json_t *section);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_BROWSERSES_71B99F_H */
