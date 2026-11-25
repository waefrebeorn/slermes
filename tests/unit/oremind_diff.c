/* Differential harness for the onboarding reminder layer.
 *
 * Cases: [ {"name": ..., "key": ..., "record": {...}, "facts": {...}, "beat": "..."|null, "seam": {...}} ]
 * Out:   [ {"name": ..., "text": "..."|null, "ported": true|false} ]
 *
 * A null text with ported=false is the port saying "this key is not ported here" (the build_* keys):
 * the oracle counts those separately so they can never pass as agreement.
 */
#include "onboarding_reminders.h"
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
    if (argc < 3) { fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]); return 2; }
    char *text = read_all(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) { fprintf(stderr, "not a JSON array\n"); return 2; }

    json_t *out = json_array();
    for (size_t i = 0; i < json_len(cases); i++) {
        const json_t *c = json_get(cases, i);
        const char *name = json_get_str(c, "name", "");
        const char *key = json_get_str(c, "key", "");
        const json_t *record = json_obj_get(c, "record");
        const json_t *facts = json_obj_get(c, "facts");
        const json_t *seam = json_obj_get(c, "seam");
        const json_t *beatv = json_obj_get(c, "beat");
        const char *beat = (beatv && beatv->type == JSON_STRING) ? beatv->str_val : NULL;

        char *rem = onboarding_reminder(key, record, facts, beat, seam);
        json_t *entry = json_object();
        json_set(entry, "name", json_string(name ? name : ""));
        if (rem) {
            json_set(entry, "text", json_string(rem));
            json_set(entry, "ported", json_bool(true));
            free(rem);
        } else {
            json_set(entry, "text", json_null());
            json_set(entry, "ported", json_bool(false));
        }
        /* The other two entry points, exercised on the same records. */
        char *note = onboarding_turn_note(record);
        json_set(entry, "turn_note", json_string(note ? note : ""));
        free(note);
        char *fin = onboarding_finish_reminder(record, seam);
        json_set(entry, "finish", json_string(fin ? fin : ""));
        free(fin);

        json_append(out, entry);
    }

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
