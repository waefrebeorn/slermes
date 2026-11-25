/* JSON-lines harness for the w23_clisubagen_f7c046 gate
 * (hermes_cli/cli_subagent_monitor.py).  One request object per stdin line; one
 * response line carrying the computed "out" plus every input key echoed. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_clisubagen_f7c046.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) {
        fputs(s, stdout);
        free(s);
    } else {
        fputs("null", stdout);
    }
}

static json_t *out_string(char *s) {
    json_t *n = json_string(s ? s : "");
    free(s);
    return n;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "clip")) {
            out = out_string(w23_clisubagen_f7c046_clip(json_obj_get(in, "value"),
                                                        (int)json_get_num(in, "width", 0)));
        } else if (!strcmp(func, "counts")) {
            out = out_string(w23_clisubagen_f7c046_counts(in));
        } else if (!strcmp(func, "title")) {
            out = out_string(w23_clisubagen_f7c046_title(in));
        } else if (!strcmp(func, "agent_activity")) {
            out = out_string(w23_clisubagen_f7c046_agent_activity(json_obj_get(in, "row")));
        } else if (!strcmp(func, "process_activity")) {
            out = out_string(w23_clisubagen_f7c046_process_activity(json_obj_get(in, "row")));
        } else if (!strcmp(func, "collapsed_activity")) {
            out = out_string(w23_clisubagen_f7c046_collapsed_activity(in));
        } else if (!strcmp(func, "dock_text")) {
            out = out_string(w23_clisubagen_f7c046_dock_text(in));
        } else if (!strcmp(func, "select")) {
            out = w23_clisubagen_f7c046_select(in);
        } else if (!strcmp(func, "view")) {
            out = w23_clisubagen_f7c046_view(in);
        } else if (!strcmp(func, "modal_prompt_active")) {
            out = json_bool(w23_clisubagen_f7c046_modal_prompt_active(json_obj_get(in, "attrs")) != 0);
        } else if (!strcmp(func, "read_tail")) {
            out = out_string(w23_clisubagen_f7c046_read_tail(json_get_str(in, "path", NULL)));
        } else if (!strcmp(func, "consts")) {
            out = w23_clisubagen_f7c046_consts();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
