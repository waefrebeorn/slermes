/* onboarding_task_catalog.c -- eligibility and the seam builders over the generated task table. */
#include "onboarding_task_catalog.h"
#include "hermes_json.h"

#include <stdbool.h>
#include <string.h>

const onboarding_task_t *onboarding_catalog_find(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < ONB_TASK_COUNT; i++)
        if (strcmp(ONB_TASKS[i].id, id) == 0) return &ONB_TASKS[i];
    return NULL;
}

const onboarding_task_t *onboarding_catalog_task_or_own(const char *id) {
    const onboarding_task_t *t = onboarding_catalog_find(id);
    return t ? t : &ONB_OWN_TASK;
}

static bool arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len((json_t *)arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0) return true;
    }
    return false;
}

bool onboarding_catalog_eligible(const onboarding_task_t *task, const json_t *facts, const json_t *picks) {
    if (!task) return false;
    const json_t *installed = picks ? json_obj_get(picks, "installed") : NULL;
    const json_t *connected = picks ? json_obj_get(picks, "connected") : NULL;
    switch (task->gate) {
        case ONB_GATE_NEVER: return false;
        case ONB_GATE_ALWAYS: return true;
        case ONB_GATE_PLUGIN: return arr_has(installed, task->gate_arg);
        case ONB_GATE_CONNECTED:
            return connected && connected->type == JSON_ARRAY && json_len((json_t *)connected) > 0;
        case ONB_GATE_OS:
            return facts && strcmp(json_get_str(facts, "os", ""), task->gate_arg) == 0;
    }
    return false;
}

json_t *onboarding_catalog_rows(const json_t *facts, const json_t *picks) {
    json_t *rows = json_array();
    for (int i = 0; i < ONB_TASK_COUNT; i++) {
        const onboarding_task_t *t = &ONB_TASKS[i];
        if (strcmp(t->id, ONB_FIXED_ID) == 0) continue;          /* the fixed row is never picked */
        if (!onboarding_catalog_eligible(t, facts, picks)) continue;
        json_t *row = json_object();
        json_set(row, "id", json_string(t->id));
        json_set(row, "label", json_string(t->label));
        json_set(row, "goal", json_string(t->goal));
        json_append(rows, row);
    }
    return rows;
}

json_t *onboarding_catalog_seam(const json_t *facts, const json_t *picks) {
    json_t *seam = json_object();
    json_set(seam, "eligible_rows", onboarding_catalog_rows(facts, picks));
    const onboarding_task_t *fixed = onboarding_catalog_find(ONB_FIXED_ID);
    json_set(seam, "fixed_label", json_string(fixed ? fixed->label : ""));
    return seam;
}
