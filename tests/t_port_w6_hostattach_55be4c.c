/* t_port_w6_hostattach_55be4c.c — differential harness for gateway/host_attach.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_hostattach_55be4c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static char *node_string(const json_t *v) {
    if (v && v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v && v->type == JSON_NUMBER) {
        char b[64];
        snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        return strdup(b);
    }
    return strdup("");
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

        if (!strcmp(func, "served")) {
            json_t *id = json_obj_get(in, "identity");
            size_t n = 0, i;
            char **arr = w6_hostattach_served_from_identity(
                id && id->type == JSON_OBJECT ? id : NULL, &n);
            json_t *a = json_array();
            for (i = 0; i < n; i++) json_append(a, json_string(arr[i]));
            out = a;
            w6_hostattach_free_strings(arr, n);
        } else if (!strcmp(func, "serves")) {
            json_t *pa = json_obj_get(in, "profiles");
            size_t cnt = (pa && pa->type == JSON_ARRAY) ? pa->c.count : 0;
            size_t i;
            char **profiles = (char **)calloc(cnt ? cnt : 1, sizeof(char *));
            for (i = 0; i < cnt; i++) profiles[i] = node_string(pa->c.items[i]);
            out = json_bool(w6_hostattach_serves(
                json_get_bool(in, "served_known", 1) ? 1 : 0,
                profiles, cnt, json_get_str(in, "profile", "")) ? 1 : 0);
            for (i = 0; i < cnt; i++) free(profiles[i]);
            free(profiles);
        } else if (!strcmp(func, "rescan_msg")) {
            char b[2048];
            w6_hostattach_standalone_rescan_message(
                json_get_str(in, "profile", ""), b, sizeof(b));
            out = json_string(b);
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
