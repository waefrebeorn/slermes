#ifndef NOC_W14_BROWSERHAR_09A2C6_H
#define NOC_W14_BROWSERHAR_09A2C6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_browserhar_09a2c6_daemon_identity @ tools/browser_harness_dirs.py:daemon_identity */
struct json_t *w14_browserhar_09a2c6_daemon_identity(const struct json_t *env);
/* PoP: w14_browserhar_09a2c6_daemon_env @ tools/browser_harness_dirs.py:daemon_env */
struct json_t *w14_browserhar_09a2c6_daemon_env(const struct json_t *env, const struct json_t *identity);
/* PoP: w14_browserhar_09a2c6_socket_dir_budget @ tools/browser_harness_dirs.py:socket_dir_budget */
struct json_t *w14_browserhar_09a2c6_socket_dir_budget(const char *platform);
/* PoP: w14_browserhar_09a2c6_fits_socket_budget @ tools/browser_harness_dirs.py:fits_socket_budget */
int w14_browserhar_09a2c6_fits_socket_budget(const char *resolved_dir, const struct json_t *budget);
/* PoP: w14_browserhar_09a2c6_harness_home @ tools/browser_harness_dirs.py:_harness_home */
struct json_t *w14_browserhar_09a2c6_harness_home(const struct json_t *env);
/* PoP: w14_browserhar_09a2c6_harness_pid_file @ tools/browser_harness_dirs.py:harness_pid_file */
struct json_t *w14_browserhar_09a2c6_harness_pid_file(const struct json_t *env, const char *suffix);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_BROWSERHAR_09A2C6_H */
