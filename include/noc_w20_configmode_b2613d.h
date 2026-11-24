/*
 * noc_w20_configmode_b2613d.h -- C11 port of the ``model:`` section load/save
 * canon from upstream hermes_cli/config_model_section.py.
 */
#ifndef NOC_W20_CONFIGMODE_B2613D_H
#define NOC_W20_CONFIGMODE_B2613D_H

#include <stddef.h>

#include "hermes_json.h"

/*
 * _normalize_root_model_keys(config) -> {"config": <normalized>, "warnings": [...]}
 *
 * ``config`` must be a JSON object; anything else is treated as {}.
 * ``warnings`` carries the ``_warn_once_per_provider`` *signatures* the upstream
 * flatten loop would emit, in call order (e.g. "discarded:default.temperature").
 */
json_t *w20_configmode_b2613d_normalize(const json_t *config);

/* Just the normalized config mapping (the "config" member of the report). */
json_t *w20_configmode_b2613d_normalize_config(const json_t *config);

/*
 * coerce_provider_id(value) as text: None -> "", strings kept, bools render
 * "True"/"False", integral numbers as decimals, other numbers as %r-ish text.
 */
char *w20_configmode_b2613d_coerce_provider(const json_t *value);

#endif /* NOC_W20_CONFIGMODE_B2613D_H */
