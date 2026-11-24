#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_modelpicke_b36083.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    char buf[512];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "clamp_fraction")) {
            out = json_number(w17_modelpicke_b36083_clamp_fraction(json_get_num(in, "value", 0.0)));
        } else if (!strcmp(func, "gauge_style")) {
            out = json_string(w17_modelpicke_b36083_gauge_style(json_get_num(in, "value", 0.0)));
        } else if (!strcmp(func, "filled_cells")) {
            long w = (long)json_get_num(in, "width", 10.0);
            out = json_number((double)w17_modelpicke_b36083_filled_cells(json_get_num(in, "value", 0.0), w));
        } else if (!strcmp(func, "render_bar")) {
            long w = (long)json_get_num(in, "width", 10.0);
            w17_modelpicke_b36083_render_bar(json_get_num(in, "value", 0.0), w, buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "format_percent_used")) {
            w17_modelpicke_b36083_format_percent_used(json_get_num(in, "value", 0.0), buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "compact_duration") || !strcmp(func, "format_resets_in")) {
            json_t *reset = json_obj_get(in, "resets_at_ms");
            json_t *now = json_obj_get(in, "now_ms");
            if (!strcmp(func, "compact_duration"))
                w17_modelpicke_b36083_compact_duration(reset, now, buf, sizeof(buf));
            else
                w17_modelpicke_b36083_format_resets_in(reset, now, buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "split_recommended")) {
            json_t *rest = NULL;
            json_t *head = w17_modelpicke_b36083_split_recommended(json_obj_get(in, "model_ids"),
                                                                   json_obj_get(in, "featured"), &rest);
            out = json_object();
            json_set(out, "recommended", head);
            json_set(out, "other", rest ? rest : json_array());
        } else if (!strcmp(func, "featured_first")) {
            out = w17_modelpicke_b36083_featured_first(json_obj_get(in, "model_ids"),
                                                       json_obj_get(in, "featured"));
        } else if (!strcmp(func, "credits_pay")) {
            int r = w17_modelpicke_b36083_credits_pay(json_obj_get(in, "info"), json_obj_get(in, "window"));
            out = r < 0 ? json_null() : json_bool(r != 0);
        } else if (!strcmp(func, "is_free_model")) {
            out = json_bool(w17_modelpicke_b36083_is_free_model(json_obj_get(in, "pricing_row")) != 0);
        } else if (!strcmp(func, "capitalize")) {
            w17_modelpicke_b36083_capitalize(json_get_str(in, "text", ""), buf, sizeof(buf));
            out = json_string(buf);
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
