/* JSON-lines harness for the w33_adapter_eaa5eb_p2 gate (IRC adapter,
 * identity / admission / config derivation). One request object per stdin
 * line; one response line carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_adapter_eaa5eb_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, json_t *in) {
    if (!strcmp(func, "privmsg_decision"))
        return w33_adapter_eaa5eb_p2_privmsg_decision(in);
    if (!strcmp(func, "next_nick_collision"))
        return w33_adapter_eaa5eb_p2_next_nick_collision(in);
    if (!strcmp(func, "standalone_nick_base"))
        return w33_adapter_eaa5eb_p2_standalone_nick_base(in);
    if (!strcmp(func, "standalone_nick"))
        return w33_adapter_eaa5eb_p2_standalone_nick(in);
    if (!strcmp(func, "nick_retry"))
        return w33_adapter_eaa5eb_p2_nick_retry(in);
    if (!strcmp(func, "standalone_target"))
        return w33_adapter_eaa5eb_p2_standalone_target(in);
    if (!strcmp(func, "standalone_lines"))
        return w33_adapter_eaa5eb_p2_standalone_lines(in);
    if (!strcmp(func, "is_channel"))
        return w33_adapter_eaa5eb_p2_is_channel(in);
    if (!strcmp(func, "chat_info"))
        return w33_adapter_eaa5eb_p2_chat_info(in);
    if (!strcmp(func, "env_enablement"))
        return w33_adapter_eaa5eb_p2_env_enablement(in);
    if (!strcmp(func, "server_channel"))
        return w33_adapter_eaa5eb_p2_server_channel(in);
    if (!strcmp(func, "config_ready"))
        return w33_adapter_eaa5eb_p2_config_ready(in);
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(func, in);
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
