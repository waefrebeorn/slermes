#ifndef NOC_W16_AUTHMODEL_E58B26_H
#define NOC_W16_AUTHMODEL_E58B26_H

#include "hermes_json.h"

/* C home for upstream hermes_cli/auth_model_picker.py.
 * Ordering and static chrome constants of the interactive model picker. */

/* _picker_order(model_ids, current_model, None) -> [ordered, sections, start] */
json_t *w16_authmodel_e58b26_picker_order(const json_t *model_ids, const char *current_model);
/* Static picker constants. */
json_t *w16_authmodel_e58b26_constants(void);

#endif /* NOC_W16_AUTHMODEL_E58B26_H */
