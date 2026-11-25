/* JSON-lines harness for the w20_store_ef85f3 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_store_ef85f3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bucket_name")) {
            json_t *idx = json_obj_get(in, "index");
            int i = (int)json_get_num(in, "index", 0);
            const char *name;
            (void)idx;
            name = w20_store_ef85f3_bucket_name(i);
            out = name ? json_string(name) : json_null();
        } else if (!strcmp(func, "is_bucket")) {
            out = json_bool(w20_store_ef85f3_is_bucket(json_get_str(in, "name", "")) ? 1 : 0);
        } else if (!strcmp(func, "load_state")) {
            out = w20_store_ef85f3_load_state(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "receipt_key")) {
            char *k = w20_store_ef85f3_receipt_key(json_obj_get(in, "notification"));
            out = json_string(k ? k : "");
            free(k);
        } else if (!strcmp(func, "upsert")) {
            out = w20_store_ef85f3_upsert(json_get_str(in, "bucket", ""),
                                          json_obj_get(in, "existing"),
                                          json_obj_get(in, "payload"),
                                          json_get_str(in, "key", ""),
                                          json_get_str(in, "now", ""));
        } else if (!strcmp(func, "record_receipt")) {
            out = w20_store_ef85f3_record_receipt(json_obj_get(in, "receipts"),
                                                  json_get_str(in, "key", ""),
                                                  json_obj_get(in, "payload"),
                                                  json_get_str(in, "received_at", ""),
                                                  json_get_str(in, "now", ""));
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
