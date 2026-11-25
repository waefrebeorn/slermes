/* t_port_w19_runnotific_a1bcca.c - harness for src/gateway/run_notifications_restart.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_runnotific_a1bcca.h"

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
        if (!strcmp(func, "notice_expired")) {
            out = w19_runnotific_a1bcca_notice_expired(json_obj_get(in, "age_seconds"),
                                                       json_get_bool(in, "stat_ok", true));
        } else if (!strcmp(func, "send_restart")) {
            json_t *spec = json_copy(in);
            json_t *exp = w19_runnotific_a1bcca_notice_expired(json_obj_get(in, "age_seconds"),
                                                              json_get_bool(in, "stat_ok", true));
            json_set(spec, "expired", exp);
            out = w19_runnotific_a1bcca_send_restart_notification(spec);
            json_free(spec);
        } else if (!strcmp(func, "deliver")) {
            out = w19_runnotific_a1bcca_deliver_restart_notification(json_obj_get(in, "data"),
                                                                    json_obj_get(in, "ctx"));
        } else if (!strcmp(func, "schedule_wanted")) {
            out = w19_runnotific_a1bcca_schedule_replay_wanted(in);
        } else if (!strcmp(func, "ttl")) {
            out = w19_runnotific_a1bcca_notice_ttl_seconds();
        } else if (!strcmp(func, "note_boot")) {
            out = w19_runnotific_a1bcca_note_restart_notice_boot(json_obj_get(in, "context_present"));
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
