/* JSON-lines harness for the w28_adapter_ca7894_p2 gate (email adapter
 * From: identity and the Authentication-Results grammar).
 *
 * One request object per stdin line: {"func": <name>, ...inputs...}.
 * One response line per request: {"func", "out", <every input key echoed>}.
 * Output is unbuffered so the oracle can stream both sides. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_adapter_ca7894_p2.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "extract_email_address", w28_adapter_ca7894_extract_email_address },
    { "domain_of",             w28_adapter_ca7894_domain_of },
    { "domains_aligned",       w28_adapter_ca7894_domains_aligned },
    { "ar_clauses",            w28_adapter_ca7894_ar_clauses },
    { "auth_props",            w28_adapter_ca7894_auth_props },
};;

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        int matched = 0;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof(W_TABLE) / sizeof(W_TABLE[0]); i++) {
            if (!strcmp(func, W_TABLE[i].name)) {
                out = W_TABLE[i].fn(in);
                matched = 1;
                break;
            }
        }
        if (!matched) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
