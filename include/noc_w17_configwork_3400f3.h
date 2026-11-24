#ifndef NOC_W17_CONFIGWORK_3400F3_H
#define NOC_W17_CONFIGWORK_3400F3_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/config_worker_save.py (gate w17_configwork_3400f3). */
json_t *w17_configwork_3400f3_schema_default_branch(const json_t *snapshot, const json_t *defaults);
/* frozen == NULL reproduces the _SAME_AS_BASE sentinel; an explicit JSON value
 * (including null) is the caller-supplied frozen branch. */
json_t *w17_configwork_3400f3_three_way(const json_t *live, const json_t *desired,
                                        const json_t *base, const json_t *frozen);

#endif /* NOC_W17_CONFIGWORK_3400F3_H */
