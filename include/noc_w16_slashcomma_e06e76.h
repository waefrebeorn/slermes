#ifndef NOC_W16_SLASHCOMMA_E06E76_H
#define NOC_W16_SLASHCOMMA_E06E76_H

#include "hermes_json.h"

/* C home for upstream gateway/slash_commands_model.py.
 * Pure data-table logic for /fast and /reasoning argument resolution. */

/* _FAST_SELECTIONS as {arg: [tier|null, persisted, i18n_label_key|null]} */
json_t *w16_slashcomma_e06e76_fast_selections_table(void);
/* _FAST_SELECTIONS.get(arg) -> [tier|null, persisted, label_key|null] or null */
json_t *w16_slashcomma_e06e76_fast_selection(const char *arg);
/* _REASONING_DISPLAY_TOGGLES as {arg: bool} */
json_t *w16_slashcomma_e06e76_reasoning_toggles_table(void);
/* _REASONING_DISPLAY_TOGGLES.get(arg) -> bool or null */
json_t *w16_slashcomma_e06e76_reasoning_toggle(const char *arg);
/* _TEXT_LISTING_MODELS */
int w16_slashcomma_e06e76_text_listing_models(void);

#endif /* NOC_W16_SLASHCOMMA_E06E76_H */
