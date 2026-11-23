#ifndef NOC_W5_LIBATOMIC_4BD942_H
#define NOC_W5_LIBATOMIC_4BD942_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_libatomic_4bd942_manager_order @ pm/libatomic.py:_manager_order
 * tokens: JSON array of lower-cased os-release tokens. Returns the ordered
 * tuple of package-manager candidates as a JSON array of strings. */
struct json_t *w5_libatomic_4bd942_manager_order(const struct json_t *tokens);

/* PoP: w5_libatomic_4bd942_host_install_command @ pm/libatomic.py:_host_install_command
 * Exposed seam: `available` is the JSON array of managers shutil.which() found.
 * Returns the installer argv as a JSON array, or JSON null when none is found. */
struct json_t *w5_libatomic_4bd942_host_install_command(const struct json_t *tokens,
                                                        const struct json_t *available);

/* PoP: w5_libatomic_4bd942_command_plan @ pm/libatomic.py:_command_plan
 * Exposed seam for _is_root()/shutil.which("sudo"). Returns an object with
 * keys argv (array or null), shown (string), remedy (string). */
struct json_t *w5_libatomic_4bd942_command_plan(const struct json_t *command,
                                                int is_root, const char *sudo_path);

/* PoP: w5_libatomic_4bd942_shlex_join @ pm/libatomic.py:shlex.join
 * Faithful POSIX shlex.quote + join over a JSON array of strings. */
struct json_t *w5_libatomic_4bd942_shlex_join(const struct json_t *tokens);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_LIBATOMIC_4BD942_H */
