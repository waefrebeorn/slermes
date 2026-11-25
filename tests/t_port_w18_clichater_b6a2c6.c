#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_clichater_b6a2c6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "short")) {
            long long limit = 120;
            json_t *lv = json_obj_get(in, "limit");
            if (lv && lv->type == JSON_NULL) limit = -1;         /* None */
            else if (lv && lv->type == JSON_NUMBER) limit = (long long)lv->num_val;
            char *s = w18_clichater_b6a2c6_short(json_get_str(in, "text", ""), limit);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "refusal_text")) {
            json_t *data = json_obj_get(in, "data");
            if (data && data->type != JSON_OBJECT) data = NULL;
            char *s = w18_clichater_b6a2c6_refusal_text(json_get_str(in, "error", ""), data);
            out = json_string(s ? s : "");
            free(s);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
