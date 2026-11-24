#ifndef NOC_W17_CONFIGSET_671393_H
#define NOC_W17_CONFIGSET_671393_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/config_set_scalars.py (gate w17_configset_671393).
 * Each function returns the coerced scalar as a JSON value: a number/bool for a
 * successful coercion, JSON null where the Python helper returns None. */
json_t *w17_configset_671393_coerce_int(const char *value);
json_t *w17_configset_671393_coerce_float(const char *value);
json_t *w17_configset_671393_quoted_bool(const char *stripped);

#endif /* NOC_W17_CONFIGSET_671393_H */
