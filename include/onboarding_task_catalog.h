#ifndef ONBOARDING_TASK_CATALOG_H
#define ONBOARDING_TASK_CATALOG_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The first-task catalog behind the build card: session_rules/onboarding_first_tasks.py.
 *
 * The table itself (prose included) is generated from the Python by scripts/gen_onboarding_catalog.py,
 * so the words cannot drift; this header is the shape plus the eligibility rules. */

typedef enum {
    ONB_GATE_NEVER = 0,   /* never offered (the fixed row) */
    ONB_GATE_ALWAYS = 1,
    ONB_GATE_PLUGIN = 2,  /* needs this plugin installed (picks.installed) */
    ONB_GATE_CONNECTED = 3, /* needs any connected app (picks.connected) */
    ONB_GATE_OS = 4,      /* needs this os */
} onboarding_gate_t;

typedef struct {
    const char *id;
    const char *label;
    onboarding_gate_t gate;
    const char *gate_arg;
    const char *const skills[8];
    const char *goal;
    const char *const steps[16];
    const char *output;
    const char *quality;
    const char *done;
} onboarding_task_t;

extern const onboarding_task_t ONB_TASKS[];
extern const int ONB_TASK_COUNT;
extern const int ONB_MAX_PICKS;
extern const int ONB_MAX_LABEL;
extern const char *const ONB_FIXED_ID;

const onboarding_task_t *onboarding_catalog_find(const char *id);

/* True when the task may be picked on this machine with these picks. */
bool onboarding_catalog_eligible(const onboarding_task_t *task, const json_t *facts, const json_t *picks);

/* [{"id","label","goal"}] in catalog order -- the seam apply() and the reminder layer take. */
json_t *onboarding_catalog_rows(const json_t *facts, const json_t *picks);

/* {"eligible_rows": [...], "fixed_label": "..."} for onboarding_reminder / onboarding_flow_apply. */
json_t *onboarding_catalog_seam(const json_t *facts, const json_t *picks);

#ifdef __cplusplus
}
#endif

#endif /* ONBOARDING_TASK_CATALOG_H */
