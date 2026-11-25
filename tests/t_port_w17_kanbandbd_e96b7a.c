#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_kanbandbd_e96b7a.h"

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
        if (!strcmp(func, "wait_status")) {
            int rc = (int)json_get_num(in, "returncode", 0.0);
            out = json_number((double)w17_kanbandbd_e96b7a_wait_status_from_returncode(rc));
        } else if (!strcmp(func, "exit_code_kind")) {
            int code = (int)json_get_num(in, "code", 0.0);
            int rlc  = (int)json_get_num(in, "rate_limit_code", 75.0);
            int tpc  = (int)json_get_num(in, "terminal_provider_code", 78.0);
            int out_code = 0;
            const char *kind = w17_kanbandbd_e96b7a_exit_code_kind(code, rlc, tpc, &out_code);
            out = json_object();
            json_set(out, "kind", json_string(kind ? kind : ""));
            json_set(out, "code", json_number((double)out_code));
        } else if (!strcmp(func, "error_fingerprint")) {
            const char *txt = json_get_str(in, "error_text", "");
            char *fp = w17_kanbandbd_e96b7a_error_fingerprint(txt);
            out = json_string(fp ? fp : "");
            free(fp);
        } else if (!strcmp(func, "worker_survived")) {
            int r = w17_kanbandbd_e96b7a_worker_survived_termination(json_obj_get(in, "termination"));
            out = json_bool(r != 0);
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
