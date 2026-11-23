#ifndef NOC_W14_AUXILIARYT_39927B_H
#define NOC_W14_AUXILIARYT_39927B_H

#include <stdbool.h>
#include "hermes_json.h"

/* Faithful C11 port of upstream agent/auxiliary_task_config.py. */

char *w14_auxiliaryt_39927b_authoritative_auxiliary_task(const char *task);
bool  w14_auxiliaryt_39927b_core_rooted(const char *base, const json_t *entries);
json_t *w14_auxiliaryt_39927b_layer_over_inherited(const json_t *inherited, const json_t *user);

#endif /* NOC_W14_AUXILIARYT_39927B_H */
