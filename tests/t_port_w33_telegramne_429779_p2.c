/* JSON-lines harness for the w33_telegramne_429779_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_telegramne_429779_p2.h"

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
        if (!strcmp(func, "normalize_fallback_ips")) {
            out = w33_telegramne_429779_normalize_fallback_ips(in);
        } else if (!strcmp(func, "cap_fallback_ips")) {
            out = w33_telegramne_429779_cap_fallback_ips(in);
        } else if (!strcmp(func, "parse_fallback_ip_env")) {
            out = w33_telegramne_429779_parse_fallback_ip_env(in);
        } else if (!strcmp(func, "is_retryable")) {
            out = w33_telegramne_429779_is_retryable(in);
        } else if (!strcmp(func, "attempt_order")) {
            out = w33_telegramne_429779_attempt_order(in);
        } else if (!strcmp(func, "connect_share")) {
            out = w33_telegramne_429779_connect_share(in);
        } else if (!strcmp(func, "request_connect_budget")) {
            out = w33_telegramne_429779_request_connect_budget(in);
        } else if (!strcmp(func, "rewrite_for_ip")) {
            out = w33_telegramne_429779_rewrite_for_ip(in);
        } else if (!strcmp(func, "doh_answer_ips")) {
            out = w33_telegramne_429779_doh_answer_ips(in);
        } else if (!strcmp(func, "discover_merge")) {
            out = w33_telegramne_429779_discover_merge(in);
        } else if (!strcmp(func, "pool_plan")) {
            out = w33_telegramne_429779_pool_plan(in);
        } else if (!strcmp(func, "walk_failed")) {
            out = w33_telegramne_429779_walk_failed(in);
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
