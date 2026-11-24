#ifndef NOC_W16_STATUSINLI_06DC54_H
#define NOC_W16_STATUSINLI_06DC54_H

#include "hermes_json.h"

/* C home for upstream gateway/status_inline_source.py.
 * Value-aware interpreter-argv walk for inline (-c) source detection. */

/* inline_source_flag_index(tokens) -> int index or JSON null */
json_t *w16_statusinli_06dc54_inline_source_flag_index(const json_t *tokens);
/* command_line_runs_inline_source(tokens) -> bool */
bool w16_statusinli_06dc54_command_line_runs_inline_source(const json_t *tokens);

#endif /* NOC_W16_STATUSINLI_06DC54_H */
