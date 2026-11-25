/* JSON-lines harness for the w33_telegramne_429779_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_telegramne_429779_p1.h"

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
        if (!strcmp(func, "env_number")) {
            out = w33_telegramne_429779_env_number(in);
        } else if (!strcmp(func, "ptb_request_kwargs")) {
            out = w33_telegramne_429779_ptb_request_kwargs(in);
        } else if (!strcmp(func, "getupdates_keepalive")) {
            out = w33_telegramne_429779_getupdates_keepalive(in);
        } else if (!strcmp(func, "keepalive_options")) {
            out = w33_telegramne_429779_keepalive_options(in);
        } else if (!strcmp(func, "proxy_target_hosts")) {
            out = w33_telegramne_429779_proxy_target_hosts(in);
        } else if (!strcmp(func, "bot_api_urls")) {
            out = w33_telegramne_429779_bot_api_urls(in);
        } else if (!strcmp(func, "verify_decision")) {
            out = w33_telegramne_429779_verify_decision(in);
        } else if (!strcmp(func, "classic_proxy_decision")) {
            out = w33_telegramne_429779_classic_proxy_decision(in);
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
