/* Differential harness for the onboarding-flow port.
 *
 * Reads a case file written by tests/oracle/onboarding_flow_oracle.py:
 *   [ {"name": ..., "facts": {...}, "record": {...}, "eligible": [...], "events": [ {...}, ... ]} ]
 * and writes:
 *   [ {"name": ..., "steps": [ {"record": {...}, "result": {...}, "reminder": "..."} ]} ]
 *
 * The comparison itself lives in the Python oracle: it is the side that knows what Python did.
 */
#include "onboarding_flow_apply.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]);
        return 2;
    }
    char *text = read_all(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) { fprintf(stderr, "cases are not a JSON array\n"); return 2; }

    json_t *out = json_array();
    for (size_t c = 0; c < json_len(cases); c++) {
        const json_t *cse = json_get(cases, c);
        const char *name = json_get_str(cse, "name", "");
        const json_t *facts = json_obj_get(cse, "facts");
        const json_t *eligible = json_obj_get(cse, "eligible");
        json_t *record = json_obj_get(cse, "record") ? json_copy(json_obj_get(cse, "record")) : json_object();
        const json_t *events = json_obj_get(cse, "events");

        json_t *steps = json_array();
        if (events && events->type == JSON_ARRAY) {
            for (size_t e = 0; e < json_len((json_t *)events); e++) {
                const json_t *ev = json_get(events, e);
                json_t *applied = onboarding_flow_apply(record, ev, facts, eligible, (double)(e + 1));
                json_t *step = json_object();
                if (!applied) {
                    json_set(step, "error", json_string("apply returned NULL"));
                } else {
                    json_set(step, "record", json_copy(json_obj_get(applied, "record")));
                    json_set(step, "result", json_copy(json_obj_get(applied, "result")));
                    const char *rem = json_get_str(applied, "reminder", "");
                    json_set(step, "reminder", json_string(rem ? rem : ""));
                    json_free(record);
                    record = json_copy(json_obj_get(applied, "record"));
                    json_free(applied);
                }
                json_append(steps, step);
            }
        }
        json_free(record);

        json_t *entry = json_object();
        json_set(entry, "name", json_string(name ? name : ""));
        json_set(entry, "steps", steps);
        json_append(out, entry);
    }

    char *ser = json_serialize(out);
    json_free(out);
    json_free(cases);
    if (!ser) { fprintf(stderr, "cannot serialize output\n"); return 2; }

    FILE *f = fopen(argv[2], "wb");
    if (!f) { free(ser); fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }
    fputs(ser, f);
    fclose(f);
    free(ser);
    return 0;
}
