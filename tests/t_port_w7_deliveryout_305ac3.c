/* t_port_w7_deliveryout_305ac3.c — differential harness for cron/delivery_outcome.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_deliveryout_305ac3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Distinguish key-absent (None) from empty string. */
static const char *str_field(const json_t *in, const char *key, bool *has) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) { *has = false; return NULL; }
    *has = true;
    return v->type == JSON_STRING ? v->str_val : "";
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        bool hstatus = false, herr = false, hjid = false, heid = false;
        const char *status = str_field(in, "status", &hstatus);
        const char *error  = str_field(in, "error", &herr);
        const char *jid    = str_field(in, "job_id", &hjid);
        const char *eid    = str_field(in, "execution_id", &heid);
        json_t *out = NULL;

        if (!strcmp(func, "terminal")) {
            out = w7_deliveryout_terminal(status ? status : "", herr, error ? error : "");
        } else if (!strcmp(func, "settled_outcome")) {
            out = w7_deliveryout_settled_outcome(status ? status : "", herr, error ? error : "");
        } else if (!strcmp(func, "settle_queued")) {
            out = json_bool(w7_deliveryout_settle_queued(hjid, jid ? jid : "",
                                                         heid, eid ? eid : "",
                                                         status ? status : "",
                                                         herr, error ? error : ""));
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
