/* JSON-lines harness for the w21_basestream_dce4b8 gate
 * (gateway/platforms/base_streaming_tts.py). One request object per stdin line;
 * one response line carrying the computed "out" plus every input key echoed. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_basestream_dce4b8.h"
#include "gateway/platforms/base_streaming_tts.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *wrap_bool(int v) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", json_bool(v != 0));
    return out;
}

static json_t *wrap_type_error(const char *type_name) {
    char msg[256];
    json_t *out = json_object();
    snprintf(msg, sizeof(msg), "argument of type '%s' is not a container or iterable", type_name);
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("TypeError"));
    json_set(out, "message", json_string(msg));
    return out;
}

/* `turn_key in completed_turns` for the shapes Python allows: a set/list of
 * strings, a str (substring test) or a dict (key membership). */
static json_t *do_should_skip(json_t *in) {
    json_t *turns = json_obj_get(in, "completed_turns");
    json_t *session_key = json_obj_get(in, "session_key");
    json_t *turn_marker = json_obj_get(in, "turn_marker");
    json_t *event = json_obj_get(in, "event");
    char *key = w21_basestream_dce4b8_turn_key(session_key, turn_marker, event);
    int found = 0;
    size_t i;
    if (!key || !key[0]) {
        free(key);
        return wrap_bool(0);
    }
    if (!turns) {
        free(key);
        return wrap_type_error("NoneType");
    }
    switch (turns->type) {
    case JSON_STRING:
        found = (turns->str_val && strstr(turns->str_val, key)) ? 1 : 0;
        break;
    case JSON_ARRAY:
        for (i = 0; i < turns->c.count; i++) {
            json_t *v = turns->c.items[i];
            if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, key)) { found = 1; break; }
        }
        break;
    case JSON_OBJECT:
        for (i = 0; i < turns->c.count; i++) {
            if (turns->c.keys[i] && !strcmp(turns->c.keys[i], key)) { found = 1; break; }
        }
        break;
    case JSON_NUMBER:
        free(key);
        return wrap_type_error("int");
    case JSON_BOOL:
        free(key);
        return wrap_type_error("bool");
    case JSON_NULL:
        free(key);
        return wrap_type_error("NoneType");
    }
    free(key);
    return wrap_bool(found);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "audio_format")) {
            out = w21_basestream_dce4b8_audio_format(json_obj_get(in, "overrides"));
        } else if (!strcmp(func, "handle")) {
            out = w21_basestream_dce4b8_handle(json_obj_get(in, "overrides"));
        } else if (!strcmp(func, "turn_key")) {
            char *s = w21_basestream_dce4b8_turn_key(json_obj_get(in, "session_key"),
                                                     json_obj_get(in, "turn_marker"),
                                                     json_obj_get(in, "event"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "should_skip")) {
            out = do_should_skip(in);
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
