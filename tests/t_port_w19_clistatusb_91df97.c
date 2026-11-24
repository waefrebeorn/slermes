#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_clistatusb_91df97.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "threshold")) {
            const char *r = w19_clistatusb_91df97_threshold_style(
                json_get_num(in, "value", 0.0), json_obj_get(in, "ladder"),
                json_get_str(in, "fallback", ""));
            out = json_string(r ? r : "");
        } else if (!strcmp(func, "finite")) {
            out = w19_clistatusb_91df97_finite(json_obj_get(in, "v"));
        } else if (!strcmp(func, "context_style")) {
            out = json_string(w19_clistatusb_91df97_context_style(json_obj_get(in, "percent")));
        } else if (!strcmp(func, "cache_style")) {
            out = json_string(w19_clistatusb_91df97_cache_style(json_get_num(in, "pct", 0.0)));
        } else if (!strcmp(func, "battery_style")) {
            out = json_string(w19_clistatusb_91df97_battery_style(json_get_str(in, "category", NULL)));
        } else if (!strcmp(func, "compression_style")) {
            out = json_string(w19_clistatusb_91df97_compression_style(json_get_num(in, "count", 0.0)));
        } else if (!strcmp(func, "context_bar")) {
            char *s = w19_clistatusb_91df97_context_bar(json_obj_get(in, "percent"),
                                                        (long long)json_get_num(in, "width", 10));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "prompt_elapsed")) {
            char *s = w19_clistatusb_91df97_prompt_elapsed(json_obj_get(in, "start"),
                                                           json_get_num(in, "duration", 0.0),
                                                           json_get_bool(in, "live", false),
                                                           json_get_num(in, "now", 0.0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "trim")) {
            char *s = w19_clistatusb_91df97_trim_text(json_get_str(in, "text", ""),
                                                      (long long)json_get_num(in, "max_width", 0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "title_badge")) {
            out = w19_clistatusb_91df97_title_badge(json_get_str(in, "title", ""),
                                                    (long long)json_get_num(in, "width", 80));
        } else if (!strcmp(func, "right_align")) {
            char *s = w19_clistatusb_91df97_right_align(json_get_str(in, "text", ""),
                                                        json_get_str(in, "title", ""),
                                                        (long long)json_get_num(in, "width", 80));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "minimal_chrome")) {
            out = json_bool(w19_clistatusb_91df97_minimal_chrome(
                (long long)json_get_num(in, "width", 80)) != 0);
        } else if (!strcmp(func, "scrollback")) {
            out = json_number((double)w19_clistatusb_91df97_scrollback_width(json_obj_get(in, "width")));
        } else if (!strcmp(func, "cache_rate")) {
            out = w19_clistatusb_91df97_cache_hit_rate(json_obj_get(in, "snapshot"),
                                                       (long long)json_get_num(in, "precision", 1));
        } else if (!strcmp(func, "goal")) {
            char *s = w19_clistatusb_91df97_goal_segment(json_obj_get(in, "snapshot"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "stash_age")) {
            char *s = w19_clistatusb_91df97_stash_age(json_get_num(in, "stashed_at", 0.0),
                                                      json_get_num(in, "now", 0.0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "field_set")) {
            out = w19_clistatusb_91df97_field_set(json_obj_get(in, "cfg"));
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
