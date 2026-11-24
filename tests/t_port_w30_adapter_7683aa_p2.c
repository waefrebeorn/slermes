/* JSON-lines harness for the w30_adapter_7683aa_p2 gate.
 * One request object per stdin line; the whole object is the argument (every
 * ported entry point reads named keys), and the response line carries the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_adapter_7683aa_p2.h"

/* The MATTERMOST_* env is a seam: the request carries the values it wants the
 * adapter to see, so every case is independent of the host environment. */
static const char *const ENV_KEYS[] = {
    "MATTERMOST_URL", "MATTERMOST_TOKEN", "MATTERMOST_ALLOWED_CHANNELS",
    "MATTERMOST_REQUIRE_MENTION", "MATTERMOST_FREE_RESPONSE_CHANNELS",
    "MATTERMOST_BOT_USERS", "MATTERMOST_REPLY_MODE", "MATTERMOST_HOME_CHANNEL",
};

static void apply_env(const json_t *in) {
    const json_t *env = json_obj_get(in, "env");
    size_t i;
    for (i = 0; i < sizeof(ENV_KEYS) / sizeof(ENV_KEYS[0]); i++) {
#ifdef _WIN32
        _putenv_s(ENV_KEYS[i], "");
#else
        unsetenv(ENV_KEYS[i]);
#endif
    }
    if (!env || env->type != JSON_OBJECT) return;
    for (i = 0; i < json_object_size(env); i++) {
        const char *k = json_object_get_key_at(env, i);
        const json_t *v = json_object_get_at(env, i);
        if (!k || !v || v->type != JSON_STRING) continue;
#ifdef _WIN32
        _putenv_s(k, v->str_val);
#else
        setenv(k, v->str_val, 1);
#endif
    }
}

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
        apply_env(in);
        if (!strcmp(func, "apply_channel_gating")) {
            out = w30_mm_p2_apply_channel_gating(in);
        } else if (!strcmp(func, "is_configured_bot_user")) {
            out = w30_mm_p2_is_configured_bot_user(in);
        } else if (!strcmp(func, "post_payload")) {
            out = w30_mm_p2_post_payload(in);
        } else if (!strcmp(func, "broken_root_fallback")) {
            out = w30_mm_p2_broken_root_fallback(in);
        } else if (!strcmp(func, "ws_event")) {
            out = w30_mm_p2_ws_event(in);
        } else if (!strcmp(func, "resolved_root_id")) {
            out = w30_mm_p2_resolved_root_id(in);
        } else if (!strcmp(func, "typing_request")) {
            out = w30_mm_p2_typing_request(in);
        } else if (!strcmp(func, "multi_image_empty")) {
            out = w30_mm_p2_multi_image_empty(in);
        } else if (!strcmp(func, "post_with_file_result")) {
            out = w30_mm_p2_post_with_file_result(in);
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
