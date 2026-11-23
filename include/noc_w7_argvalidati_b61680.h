#ifndef NOC_W7_ARGVALIDATI_B61680_H
#define NOC_W7_ARGVALIDATI_B61680_H

#include "hermes_json.h"

/* Public C API for the pure argument-check helpers of tools/arg_validation.py
 * (gate w7_argvalidati_b61680). argval_ prefix avoids collisions with the
 * other arg_* symbols in this repo. */
struct json_t *argval_with_choices(const char *key, const struct json_t *properties);
/* Returns [missing, unknown] (both arrays of names). */
struct json_t *argval_missing_unknown(const struct json_t *params, const struct json_t *args,
                                      const struct json_t *compat_args,
                                      const struct json_t *self_checked_args);
struct json_t *argval_flag_unknown_args(const char *result, const char *name,
                                        const struct json_t *unknown, const char *valid);

#endif /* NOC_W7_ARGVALIDATI_B61680_H */
