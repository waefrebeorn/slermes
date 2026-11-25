/* JSON-lines harness for the w22_sessionwor_277372 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_sessionwor_277372.h"

/* Python str() of what the OS returns: a number prints as an integer. */
static char uid_buf[64];
static const char *node_text(const json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_NUMBER) {
        long long n = (long long)v->num_val;
        if ((double)n == v->num_val) { snprintf(uid_buf, sizeof(uid_buf), "%lld", n); return uid_buf; }
        snprintf(uid_buf, sizeof(uid_buf), "%g", v->num_val);
        return uid_buf;
    }
    return NULL;
}

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "verify")) {
            out = w22_sessionwor_277372_verify(json_obj_get(in, "world"));
        } else if (!strcmp(func, "owner_subject")) {
            char *s = w22_sessionwor_277372_owner_subject(json_get_str(in, "os_name", NULL),
                                                          node_text(json_obj_get(in, "uid")),
                                                          node_text(json_obj_get(in, "sid")));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "reserve")) {
            out = w22_sessionwor_277372_reserve(in);
        } else if (!strcmp(func, "lose")) {
            out = w22_sessionwor_277372_lose(in);
        } else if (!strcmp(func, "hops")) {
            out = json_number((double)w22_sessionwor_277372_hops());
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
