#ifndef NOC_W16_CLIINLINE_FA1B6A_H
#define NOC_W16_CLIINLINE_FA1B6A_H

#include "hermes_json.h"

/* C home for upstream hermes_cli/cli_inline_images.py.
 * Scrollback-safe inline image row rendering. */

/* _zero_width(seq) -> "\x01" + seq + "\x02" (as a JSON string) */
json_t *w16_cliinline_fa1b6a_zero_width(const char *seq);
/* image_rows(image) -> [[row, raw], ...] */
json_t *w16_cliinline_fa1b6a_image_rows(const json_t *image);
/* Static inline-image constants. */
json_t *w16_cliinline_fa1b6a_constants(void);

#endif /* NOC_W16_CLIINLINE_FA1B6A_H */
