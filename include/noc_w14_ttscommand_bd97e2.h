#ifndef NOC_W14_TTSCOMMAND_BD97E2_H
#define NOC_W14_TTSCOMMAND_BD97E2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tools/tts_command_provider.py (gate w14_ttscommand_bd97e2).
 * All results are freshly allocated json_t values freed by the caller.
 * `os_name` is passed explicitly ("posix"/"nt") because the oracle runs on Windows
 * where os.name == "nt"; fixtures pin the branch instead of reading the host OS. */

/* PoP: w14_ttscommand_bd97e2_shell_quote_context @ tools/tts_command_provider.py:shell_quote_context */
struct json_t *w14_ttscommand_bd97e2_shell_quote_context(const struct json_t *command_template,
                                                        int position);

/* PoP: w14_ttscommand_bd97e2_quote_command_placeholder @ tools/tts_command_provider.py:quote_command_placeholder
 * Windows-unsafe values (ValueError in the live function) are not exercised by
 * the fixtures; safe quote contexts and shlex.quote/list2cmdline are ported. */
struct json_t *w14_ttscommand_bd97e2_quote_command_placeholder(const struct json_t *value,
                                                              const struct json_t *quote_context,
                                                              const char *os_name);

/* PoP: w14_ttscommand_bd97e2_render_command_template @ tools/tts_command_provider.py:render_command_template */
struct json_t *w14_ttscommand_bd97e2_render_command_template(const struct json_t *command_template,
                                                            const struct json_t *placeholders,
                                                            const char *os_name);

/* PoP: w14_ttscommand_bd97e2_command_failure_detail @ tools/tts_command_provider.py:command_failure_detail */
struct json_t *w14_ttscommand_bd97e2_command_failure_detail(const struct json_t *err_text,
                                                            const struct json_t *out_text);

/* PoP: w14_ttscommand_bd97e2_get_provider_section @ tools/tts_command_provider.py:_get_provider_section */
struct json_t *w14_ttscommand_bd97e2_get_provider_section(const struct json_t *config,
                                                         const char *name);

/* PoP: w14_ttscommand_bd97e2_is_command_provider_config @ tools/tts_command_provider.py:_is_command_provider_config */
struct json_t *w14_ttscommand_bd97e2_is_command_provider_config(const struct json_t *config);

/* PoP: w14_ttscommand_bd97e2_named_provider_config @ tools/tts_command_provider.py:_named_provider_config
 * `builtins` is a JSON array of reserved provider names. */
struct json_t *w14_ttscommand_bd97e2_named_provider_config(const struct json_t *config,
                                                          const char *name,
                                                          const struct json_t *builtins);

/* PoP: w14_ttscommand_bd97e2_resolve_command_config @ tools/tts_command_provider.py:_resolve_command_config */
struct json_t *w14_ttscommand_bd97e2_resolve_command_config(const struct json_t *provider,
                                                           const struct json_t *config,
                                                           const struct json_t *reserved);

/* PoP: w14_ttscommand_bd97e2_command_timeout @ tools/tts_command_provider.py:_command_timeout */
struct json_t *w14_ttscommand_bd97e2_command_timeout(const struct json_t *config,
                                                    double default_value);

/* PoP: w14_ttscommand_bd97e2_command_output_format @ tools/tts_command_provider.py:_command_output_format
 * `formats` is a JSON array of valid format names. */
struct json_t *w14_ttscommand_bd97e2_command_output_format(const struct json_t *config,
                                                          const struct json_t *formats,
                                                          const char *default_value);

/* PoP: w14_ttscommand_bd97e2_get_command_tts_output_format @ tools/tts_command_provider.py:_get_command_tts_output_format */
struct json_t *w14_ttscommand_bd97e2_get_command_tts_output_format(const struct json_t *config,
                                                                  const struct json_t *output_path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TTSCOMMAND_BD97E2_H */
