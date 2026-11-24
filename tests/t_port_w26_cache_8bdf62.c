/* JSON-lines harness for the w26_cache_8bdf62 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_cache_8bdf62.h"

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
        if (!strcmp(func, "fingerprint")) {
            out = w26_cache_8bdf62_fingerprint(in);
        } else if (!strcmp(func, "is_fresh")) {
            out = w26_cache_8bdf62_is_fresh(in);
        } else if (!strcmp(func, "entry_from_payload")) {
            out = w26_cache_8bdf62_entry_from_payload(in);
        } else if (!strcmp(func, "disk_read")) {
            out = w26_cache_8bdf62_disk_read(in);
        } else if (!strcmp(func, "write_payload")) {
            out = w26_cache_8bdf62_write_payload(in);
        } else if (!strcmp(func, "lookup")) {
            out = w26_cache_8bdf62_lookup(in);
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
