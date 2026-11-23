#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_publication_1af7eb.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = json_object();
        if (!strcmp(func, "metadata_records")) {
            json_t *d = json_obj_get(in, "data");
            const char *data = (d && d->type == JSON_STRING) ? d->str_val : NULL;
            json_t *parsed = NULL;
            if (w5_publication_1af7eb_metadata_records(data, &parsed)) {
                json_set(out, "result", parsed);
            } else {
                json_set(out, "error",
                         json_string("Plugin install metadata changed while preparing the update; retry."));
            }
        } else {
            json_set(out, "error", json_string("UNKNOWN_FUNC"));
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
