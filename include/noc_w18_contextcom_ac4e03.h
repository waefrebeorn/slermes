#ifndef NOC_W18_CONTEXTCOM_AC4E03_H
#define NOC_W18_CONTEXTCOM_AC4E03_H

#include "hermes_json.h"

/* C home for upstream Hermes agent/context_compressor_prune.py (pure helpers). */

json_t *w18_contextcom_ac4e03_changed_rows(const json_t *messages, const json_t *pruned);
json_t *w18_contextcom_ac4e03_keep_running_turn(const json_t *messages, const json_t *pruned,
                                                const json_t *turn_start);
json_t *w18_contextcom_ac4e03_would_prune(const json_t *tokens, const json_t *current_tokens);
json_t *w18_contextcom_ac4e03_constants(void);

#endif /* NOC_W18_CONTEXTCOM_AC4E03_H */
