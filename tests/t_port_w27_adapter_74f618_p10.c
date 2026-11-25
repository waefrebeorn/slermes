/* JSON-lines harness for the w27_adapter_74f618_p10 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p10.h"

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
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "require_mention")) out = w27_p10_require_mention(in);
        else if (!strcmp(func, "extra_or_env_flag")) out = w27_p10_extra_or_env_flag(in);
        else if (!strcmp(func, "addressed_other_user")) out = w27_p10_addressed_other_user(in);
        else if (!strcmp(func, "is_free_channel")) out = w27_p10_is_free_channel(in);
        else if (!strcmp(func, "reply_expected")) out = w27_p10_reply_expected(in);
        else if (!strcmp(func, "message_mentions_self")) out = w27_p10_message_mentions_self(in);
        else if (!strcmp(func, "extra_or_env_channel_set")) out = w27_p10_extra_or_env_channel_set(in);
        else if (!strcmp(func, "slash_channel_gated")) out = w27_p10_slash_channel_gated(in);
        else if (!strcmp(func, "is_slack_cdn_url")) out = w27_p10_is_slack_cdn_url(in);
        else if (!strcmp(func, "thread_rehydration_key")) out = w27_p10_thread_rehydration_key(in);
        else out = json_string("UNKNOWN_FUNC");
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
