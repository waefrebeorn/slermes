/* JSON-lines harness for the w30_adapter_7683aa_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_adapter_7683aa_p1.h"

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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        apply_env(in);
        if (!strcmp(func, "format_message")) {
            out = w30_mm_p1_format_message(json_get_str(in, "content", ""));
        } else if (!strcmp(func, "with_mentions_disabled")) {
            out = w30_mm_p1_with_mentions_disabled(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "channel_id_set")) {
            out = w30_mm_p1_channel_id_set(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "post_result")) {
            out = w30_mm_p1_post_result(json_obj_get(in, "data"), json_get_str(in, "error", ""));
        } else if (!strcmp(func, "url_filename")) {
            out = w30_mm_p1_url_filename(json_get_str(in, "url", ""),
                                         json_get_str(in, "fallback", ""));
        } else if (!strcmp(func, "channel_info")) {
            out = w30_mm_p1_channel_info(json_obj_get(in, "data"), json_get_str(in, "chat_id", ""));
        } else if (!strcmp(func, "api_request_url")) {
            out = w30_mm_p1_api_request_url(json_get_str(in, "base_url", ""),
                                            json_get_str(in, "path", ""));
        } else if (!strcmp(func, "auth_header")) {
            out = w30_mm_p1_auth_header(json_get_str(in, "token", ""));
        } else if (!strcmp(func, "request_headers")) {
            out = w30_mm_p1_request_headers(json_get_str(in, "token", ""));
        } else if (!strcmp(func, "broken_thread_root")) {
            out = w30_mm_p1_broken_thread_root(json_obj_get(in, "status"),
                                               json_get_str(in, "error", ""));
        } else if (!strcmp(func, "reconnect_step")) {
            out = w30_mm_p1_reconnect_step(json_obj_get(in, "delay"), json_obj_get(in, "rnd"));
        } else if (!strcmp(func, "url_and_token")) {
            out = w30_mm_p1_url_and_token(json_obj_get(in, "config"));
        } else if (!strcmp(func, "validate_config")) {
            out = w30_mm_p1_validate_config(json_obj_get(in, "config"));
        } else if (!strcmp(func, "constants")) {
            out = w30_mm_p1_constants();
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
