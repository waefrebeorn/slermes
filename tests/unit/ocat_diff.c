/* Differential harness for the generated first-task catalog. */
#include "onboarding_task_catalog.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]); return 2; }
    char *text = read_all(argv[1]);
    if (!text) return 2;
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) return 2;

    json_t *out = json_object();
    json_set(out, "max_picks", json_number(ONB_MAX_PICKS));
    json_set(out, "max_label", json_number(ONB_MAX_LABEL));
    json_set(out, "fixed_id", json_string(ONB_FIXED_ID));

    json_t *every = json_array();
    for (int i = 0; i < ONB_TASK_COUNT; i++) {
        const onboarding_task_t *t = &ONB_TASKS[i];
        json_t *o = json_object();
        json_set(o, "id", json_string(t->id));
        json_set(o, "label", json_string(t->label));
        json_set(o, "goal", json_string(t->goal));
        json_set(o, "output", json_string(t->output));
        json_set(o, "quality", json_string(t->quality));
        json_set(o, "done", json_string(t->done));

        json_t *skills = json_array();
        for (int k = 0; t->skills[k]; k++) json_append(skills, json_string(t->skills[k]));
        json_set(o, "skills", skills);

        json_t *steps = json_array();
        for (int k = 0; t->steps[k]; k++) json_append(steps, json_string(t->steps[k]));
        json_set(o, "steps", steps);

        json_append(every, o);
    }
    json_set(out, "tasks", every);

    json_t *results = json_array();
    for (size_t i = 0; i < json_len(cases); i++) {
        const json_t *c = json_get(cases, i);
        const json_t *facts = json_obj_get(c, "facts");
        const json_t *picks = json_obj_get(c, "picks");
        json_t *entry = json_object();
        json_set(entry, "name", json_string(json_get_str(c, "name", "")));
        json_set(entry, "rows", onboarding_catalog_rows(facts, picks));
        json_t *eligible = json_array();
        for (int k = 0; k < ONB_TASK_COUNT; k++)
            if (onboarding_catalog_eligible(&ONB_TASKS[k], facts, picks))
                json_append(eligible, json_string(ONB_TASKS[k].id));
        json_set(entry, "eligible", eligible);
        json_append(results, entry);
    }
    json_set(out, "cases", results);

    char *ser = json_serialize(out);
    json_free(out);
    json_free(cases);
    if (!ser) return 2;
    FILE *f = fopen(argv[2], "wb");
    if (!f) { free(ser); return 2; }
    fputs(ser, f);
    fclose(f);
    free(ser);
    return 0;
}
