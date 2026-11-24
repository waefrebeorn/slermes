#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_mainplatfo_97fb7b.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "yes_no")) {
            json_t *rv = json_obj_get(in, "response");
            const char *resp = (rv && rv->type == JSON_STRING) ? rv->str_val : NULL;
            out = json_bool(mainplatfo_yes_no(resp) ? 1 : 0);
        } else if (!strcmp(func, "whatsapp_mode")) {
            json_t *cv = json_obj_get(in, "current_mode");
            json_t *ch = json_obj_get(in, "choice");
            const char *cm = (cv && cv->type == JSON_STRING) ? cv->str_val : NULL;
            const char *choice = (ch && ch->type == JSON_STRING) ? ch->str_val : NULL;
            char *s = mainplatfo_whatsapp_mode(cm, choice);
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "paired")) {
            out = json_bool(mainplatfo_has_paired_identity(json_obj_get(in, "creds")) ? 1 : 0);
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
