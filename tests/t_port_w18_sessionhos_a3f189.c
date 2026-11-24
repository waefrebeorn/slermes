/*
 * t_port_w18_sessionhos_a3f189.c — JSON-lines harness for the C11 port of
 * gateway/session_hosted_members.py (gate w18_sessionhos_a3f189).
 *
 * One request per stdin line, one response line per request:
 *   {"func":<name>,"out":<value>, ...every input key echoed...}
 * Missing keys never crash: they take the Python defaults (None/""/True/[]).
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_sessionhos_a3f189.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(json_t *in, const char *func) {
    json_t *out = NULL;

    if (!strcmp(func, "member_roster")) {
        const char *profile_id = json_get_str(in, "profile_id", "");
        json_t *bots = json_obj_get(in, "bots");
        int siblings = json_get_bool(in, "siblings", true) ? 1 : 0;
        json_t *config = json_obj_get(in, "config");
        json_t *homes = NULL, *refused = NULL, *reports = NULL;
        w18_sessionhos_a3f189_member_roster(profile_id, bots, siblings, config,
                                            &homes, &refused, &reports);
        out = json_object();
        json_set(out, "homes", homes ? homes : json_object());
        json_set(out, "refused", refused ? refused : json_object());
        json_set(out, "reports", reports ? reports : json_array());
    } else if (!strcmp(func, "served")) {
        const char *profile_id = json_get_str(in, "profile_id", "");
        json_t *bots = json_obj_get(in, "bots");
        char *own = NULL;
        json_t *homes = NULL, *refused = NULL;
        w18_sessionhos_a3f189_served(profile_id, bots, &own, &homes, &refused);
        out = json_object();
        json_set(out, "own", own ? json_string(own) : json_null());
        json_set(out, "homes", homes ? homes : json_object());
        json_set(out, "refused", refused ? refused : json_object());
        free(own);
    } else if (!strcmp(func, "configured")) {
        const char *own = json_get_str(in, "own", "");
        json_t *config = json_obj_get(in, "config");
        json_t *reports = NULL;
        json_t *profiles = w18_sessionhos_a3f189_configured(own, config, &reports);
        out = json_object();
        json_set(out, "profiles", profiles ? profiles : json_object());
        json_set(out, "reports", reports ? reports : json_array());
    } else if (!strcmp(func, "config_entry")) {
        out = w18_sessionhos_a3f189_config_entry(json_obj_get(in, "name"),
                                                 json_obj_get(in, "value"),
                                                 json_obj_get(in, "served"),
                                                 json_obj_get(in, "seated"));
    } else if (!strcmp(func, "report")) {
        const char *own = json_get_str(in, "own", "");
        const char *field = json_get_str(in, "field", "");
        const char *detail = json_get_str(in, "detail", "");
        json_t *value = json_obj_get(in, "value");
        int logged = w18_sessionhos_a3f189_report(own, field, value, detail);
        out = json_object();
        json_set(out, "logged", json_bool(logged ? 1 : 0));
    } else if (!strcmp(func, "refuse_unseatable")) {
        out = w18_sessionhos_a3f189_refuse_unseatable(json_obj_get(in, "members"),
                                                      json_obj_get(in, "refused"));
    } else if (!strcmp(func, "path_str")) {
        const char *value = json_get_str(in, "value", "");
        char *s = w18_sessionhos_a3f189_path_str(value);
        out = s ? json_string(s) : json_null();
        free(s);
    } else if (!strcmp(func, "home_key")) {
        const char *value = json_get_str(in, "value", "");
        char *s = w18_sessionhos_a3f189_home_key(value);
        out = s ? json_string(s) : json_null();
        free(s);
    } else if (!strcmp(func, "served_profile_name")) {
        const char *value = json_get_str(in, "value", "");
        char *s = w18_sessionhos_a3f189_served_profile_name(value);
        out = s ? json_string(s) : json_null();
        free(s);
    } else if (!strcmp(func, "identifier_ok")) {
        const char *name = json_get_str(in, "name", "");
        out = json_bool(w18_sessionhos_a3f189_identifier_ok(name) ? 1 : 0);
    } else {
        out = json_string("UNKNOWN_FUNC");
    }
    return out;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = dispatch(in, func);
        json_t *func_node = json_string(func);
        printf("{");
        emit_key("func", func_node);
        json_free(func_node);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            json_t *copy;
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            copy = json_copy(json_object_get_at(in, i));
            emit_key(k, copy);
            json_free(copy);
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
