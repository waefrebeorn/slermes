/* JSON-lines harness for the w29_adapter_d36d4e_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_d36d4e_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "parse_comma_list")) {
            out = w29_adapter_d36d4e_p1_parse_comma_list(json_obj_get(in, "value"));
        } else if (!strcmp(func, "guess_extension")) {
            out = w29_adapter_d36d4e_p1_guess_extension(json_obj_get(in, "data_b64"));
        } else if (!strcmp(func, "is_image_ext")) {
            out = w29_adapter_d36d4e_p1_is_image_ext(json_obj_get(in, "ext"));
        } else if (!strcmp(func, "is_audio_ext")) {
            out = w29_adapter_d36d4e_p1_is_audio_ext(json_obj_get(in, "ext"));
        } else if (!strcmp(func, "ext_to_mime")) {
            out = w29_adapter_d36d4e_p1_ext_to_mime(json_obj_get(in, "ext"));
        } else if (!strcmp(func, "render_mentions")) {
            out = w29_adapter_d36d4e_p1_render_mentions(json_obj_get(in, "text"),
                                                        json_obj_get(in, "mentions"));
        } else if (!strcmp(func, "is_signal_service_id")) {
            out = w29_adapter_d36d4e_p1_is_signal_service_id(json_obj_get(in, "value"));
        } else if (!strcmp(func, "looks_like_e164_number")) {
            out = w29_adapter_d36d4e_p1_looks_like_e164_number(json_obj_get(in, "value"));
        } else if (!strcmp(func, "validate_signal_config")) {
            out = w29_adapter_d36d4e_p1_validate_signal_config(json_obj_get(in, "extra"),
                                                               json_obj_get(in, "env"));
        } else if (!strcmp(func, "check_signal_requirements")) {
            out = w29_adapter_d36d4e_p1_check_signal_requirements();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        {
            char *f = json_dumps(json_string(func), 0);
            printf("\"func\":%s", f ? f : "\"\"");
            if (f) free(f);
        }
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
