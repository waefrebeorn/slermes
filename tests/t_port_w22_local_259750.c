/* JSON-lines harness for the w22_local_259750 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_local_259750.h"
#include "agent/vault_backends/local.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* The clock seam travels as the decimal text the port parses; a numeric
 * fixture value is rendered exactly (epoch seconds fit an int64). */
static void at_text(const json_t *v, char *out, size_t cap) {
    out[0] = '\0';
    if (!v) return;
    if (v->type == JSON_STRING) {
        snprintf(out, cap, "%s", v->str_val ? v->str_val : "");
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(out, cap, "%lld", (long long)v->num_val);
        } else {
            snprintf(out, cap, "%.17g", v->num_val);
        }
    }
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *handle;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        handle = json_get_str(in, "handle", NULL);
        if (!strcmp(func, "info")) {
            json_t *info = w22_local_259750_backend_info();
            out = json_object();
            json_set(out, "ok", json_bool(1));
            json_set(out, "value", info);
        } else if (!strcmp(func, "list")) {
            out = w22_local_259750_list_items(json_obj_get(in, "records"));
        } else if (!strcmp(func, "meta")) {
            out = w22_local_259750_get_meta(json_obj_get(in, "records"), handle);
        } else if (!strcmp(func, "password")) {
            out = w22_local_259750_resolve_password(json_obj_get(in, "records"), handle);
        } else if (!strcmp(func, "secret")) {
            out = w22_local_259750_resolve_secret(json_obj_get(in, "records"), handle);
        } else if (!strcmp(func, "otp")) {
            char at[64];
            at_text(json_obj_get(in, "at"), at, sizeof(at));
            out = w22_local_259750_resolve_otp(json_obj_get(in, "records"), handle,
                                               at[0] ? at : NULL);
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
