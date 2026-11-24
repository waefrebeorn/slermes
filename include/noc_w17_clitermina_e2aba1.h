#ifndef NOC_W17_CLITERMINA_E2ABA1_H
#define NOC_W17_CLITERMINA_E2ABA1_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/cli_terminal_input.py (gate w17_clitermina_e2aba1).
 * Environment/config/terminal-width inputs are injected explicitly so the
 * pure helpers can be exercised without a live terminal. */
int w17_clitermina_e2aba1_is_ghostty(const json_t *env);
int w17_clitermina_e2aba1_supports_extended_enter_keys(const json_t *env);
int w17_clitermina_e2aba1_multiline_shortcuts_enabled(const json_t *config);
int w17_clitermina_e2aba1_is_backslash_line_continuation(const char *text);
json_t *w17_clitermina_e2aba1_apply_backslash_line_continuation(const char *text);
int w17_clitermina_e2aba1_should_auto_attach_clipboard_image(const char *pasted_text);
int w17_clitermina_e2aba1_status_bar_visible(const json_t *config);
json_t *w17_clitermina_e2aba1_split_path_input(const char *raw);

#endif /* NOC_W17_CLITERMINA_E2ABA1_H */
