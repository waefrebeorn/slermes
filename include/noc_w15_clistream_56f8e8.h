#ifndef NOC_W15_CLISTREAM_56F8E8_H
#define NOC_W15_CLISTREAM_56F8E8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/cli_stream_mixin.py (gate w15_clistream_56f8e8).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_clistream_56f8e8_slow_command_status @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._slow_command_status
 * Translation keys are returned verbatim; the oracle stubs t() as identity. */
struct json_t *w15_clistream_56f8e8_slow_command_status(const char *command);

/* PoP: w15_clistream_56f8e8_current_reasoning_callback @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._current_reasoning_callback
 * Returns "stream", "on_reasoning" or null for the selected callback. */
struct json_t *w15_clistream_56f8e8_current_reasoning_callback(const struct json_t *show_reasoning,
                                                              const struct json_t *streaming_enabled,
                                                              const struct json_t *verbose);

/* PoP: w15_clistream_56f8e8_reset_stream_state @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._reset_stream_state
 * Returns the post-reset state mapping. */
struct json_t *w15_clistream_56f8e8_reset_stream_state(void);

/* PoP: w15_clistream_56f8e8_flush_reasoning_preview @ hermes_cli/cli_stream_mixin.py:CLIStreamMixin._flush_reasoning_preview
 * `columns` injects _terminal_columns(); t() is identity so the prefix is fixed.
 * Returns {"buf": <remaining>, "flush": <flushed text or null>}. */
struct json_t *w15_clistream_56f8e8_flush_reasoning_preview(const char *buf,
                                                            int force,
                                                            int columns);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CLISTREAM_56F8E8_H */
