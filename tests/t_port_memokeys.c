#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_memokeys.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int is_null(const json_t *in, const char *key) {
    return json_is_null(json_obj_get(in, key));
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *normalized = json_get_str(in, "normalized", "");
        json_t *out = NULL;

        if (!strcmp(func, "memo_key")) {
            int an = is_null(in, "api_key");
            char *norm = NULL, *slot = NULL;
            memokeys_endpoint_memo_key(normalized,
                                       an ? "" : json_get_str(in, "api_key", ""),
                                       an, &norm, &slot);
            out = json_object();
            json_set(out, "normalized", json_string(norm));
            json_set(out, "slot", json_string(slot));
            free(norm); free(slot);
        } else if (!strcmp(func, "disk_slot")) {
            int sn = is_null(in, "slot");
            char *s = memokeys_endpoint_disk_slot(normalized,
                                                  sn ? NULL : json_get_str(in, "slot", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "fingerprint")) {
            int vn = is_null(in, "value");
            char *fp = memokeys_fingerprint(vn ? NULL : json_get_str(in, "value", ""));
            out = fp ? json_string(fp) : json_null();
            free(fp);
        } else if (!strcmp(func, "minted")) {
            char *s = memokeys_minted_slot(json_get_str(in, "id", "0"));
            out = json_string(s ? s : "");
            free(s);
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
