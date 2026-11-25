/* JSON-lines harness for the w27_adapter_04abb5_p1 gate (Buzz adapter
 * text / url / identity leaves).
 *
 * One request object per stdin line: {"func": <name>, ...inputs...}.
 * One response line per request: {"func", "out", <every input key echoed>}.
 * Output is unbuffered so the oracle can stream both sides. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_04abb5_p1.h"

typedef json_t *(*w_fn)(const json_t *);

static json_t *w_constants_adapter(const json_t *in) { (void)in; return w27_adapter_04abb5_constants(); }

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "escape_mention",      w27_adapter_04abb5_escape_mention },
    { "safe_filename",       w27_adapter_04abb5_safe_filename },
    { "attachment_origin",   w27_adapter_04abb5_attachment_origin },
    { "effective_port",      w27_adapter_04abb5_effective_port },
    { "is_relay_media_url",  w27_adapter_04abb5_is_relay_media_url },
    { "find_relay_media_refs", w27_adapter_04abb5_find_relay_media_refs },
    { "replace_media_refs",  w27_adapter_04abb5_replace_media_refs },
    { "hex_to_npub",         w27_adapter_04abb5_hex_to_npub },
    { "constants",           w_constants_adapter },
};

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
        json_t *in, *out;
        const char *func;
        size_t i, t;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in || in->type != JSON_OBJECT) { if (in) json_free(in); continue; }
        func = json_get_str(in, "func", "");
        out = NULL;
        for (t = 0; t < sizeof(W_TABLE) / sizeof(W_TABLE[0]); t++) {
            if (!strcmp(func, W_TABLE[t].name)) { out = W_TABLE[t].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
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
