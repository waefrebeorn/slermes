/* t_port_w14_imageevict_e64946.c — JSON-lines harness for image_eviction_policy.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_imageevict_e64946.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *str_arg(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool bool_arg(const json_t *in, const char *key, bool def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "image_dimensions_from_bytes")) {
            out = w14_imageevict_e64946_image_dimensions_from_bytes(str_arg(in, "raw_b64"));
        } else if (!strcmp(func, "image_max_side_from_data")) {
            long long mx = -1;
            bool ok = w14_imageevict_e64946_image_max_side_from_data(str_arg(in, "data"), &mx);
            out = ok ? json_number((double)mx) : json_null();
        } else if (!strcmp(func, "image_within_many_image_limit")) {
            out = json_bool(w14_imageevict_e64946_image_within_many_image_limit(str_arg(in, "data")));
        } else if (!strcmp(func, "images_all_within_many_image_limit")) {
            out = json_bool(w14_imageevict_e64946_images_all_within_many_image_limit(json_obj_get(in, "datas")));
        } else if (!strcmp(func, "outbound_image_block_limit")) {
            out = json_number((double)w14_imageevict_e64946_outbound_image_block_limit(
                bool_arg(in, "many_image_safe", false)));
        } else if (!strcmp(func, "outbound_image_retire_count")) {
            long long lim = (long long)json_get_num(in, "limit", -1);
            out = json_number((double)w14_imageevict_e64946_outbound_image_retire_count(
                json_obj_get(in, "carrier_blocks"), (long long)json_get_num(in, "reserved_blocks", 0),
                json_obj_get(in, "carrier_bytes"), (long long)json_get_num(in, "reserved_bytes", 0),
                bool_arg(in, "many_image_safe", false), lim,
                (long long)json_get_num(in, "budget", 24000000.0),
                (long long)json_get_num(in, "batch", 8.0),
                (long long)json_get_num(in, "floor", 3.0)));
        } else if (!strcmp(func, "latched_retire_count")) {
            out = w14_imageevict_e64946_latched_retire_count(
                (long long)json_get_num(in, "retire", 0), json_obj_get(in, "ids"),
                json_obj_get(in, "retired"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
