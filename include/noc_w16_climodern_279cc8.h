#ifndef NOC_W16_CLIMODERN_279CC8_H
#define NOC_W16_CLIMODERN_279CC8_H

#include "hermes_json.h"

/* C home for upstream hermes_cli/cli_modern_layout.py.
 * Pure display-config and ANSI-row helpers of the glyph layout. */

json_t *w16_climodern_279cc8_layout_from_config(const json_t *display);
json_t *w16_climodern_279cc8_busy_input_mode_from_config(const json_t *display);
json_t *w16_climodern_279cc8_fg(const char *hex_color, bool bold);
json_t *w16_climodern_279cc8_reasoning_text_line(const char *line);
json_t *w16_climodern_279cc8_constants(void);

#endif /* NOC_W16_CLIMODERN_279CC8_H */
