/* JSON-lines harness for the w22_telegramsp_8c7e95 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_telegramsp_8c7e95.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Optional int knob: absent key keeps `def`. */
static int opt_int(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (int)v->num_val;
    return def;
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "strip_mdv2")) {
            char *s = w22_telegramsp_8c7e95_strip_mdv2(json_get_str(in, "text", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "quotable")) {
            char *s = w22_telegramsp_8c7e95_quotable_text(json_get_str(in, "text", NULL),
                                                          opt_int(in, "mdv2", 0));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "anchor")) {
            out = w22_telegramsp_8c7e95_anchor(opt_int(in, "anchor_multipart", -1),
                                               opt_int(in, "parts", 1),
                                               opt_int(in, "dm_topic", 0),
                                               json_obj_get(in, "reply_to"));
        } else if (!strcmp(func, "const")) {
            out = json_number((double)W22_TELEGRAMSP_8C7E95_REPLY_FILE_MAX_BYTES);
        } else {
            out = json_string("UNKNOWN_FUNC");
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
