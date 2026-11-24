#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_ssh_5f7591.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return def;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "sync_error")) {
            out = w19_ssh_5f7591_sync_error(json_get_str(in, "reason", ""),
                                            json_get_str(in, "subject", ""),
                                            json_get_str(in, "what", "the SSH connection"));
        } else if (!strcmp(func, "private_runtime_dir")) {
            out = json_bool(w19_ssh_5f7591_private_runtime_dir(
                jbool(in, "lstat_fails", 0), (long long)json_get_num(in, "mode", 0.0),
                (long long)json_get_num(in, "uid", -1.0), (long long)json_get_num(in, "my_uid", 0.0)) ? true : false);
        } else if (!strcmp(func, "control_socket_for")) {
            char *p = w19_ssh_5f7591_control_socket_for(json_get_str(in, "control_socket", ""),
                                                        json_obj_get(in, "send_env"));
            out = p ? json_string(p) : json_null();
            free(p);
        } else if (!strcmp(func, "target_flags")) {
            out = w19_ssh_5f7591_target_flags(json_get_str(in, "port_flag", "-p"),
                                              (long long)json_get_num(in, "port", 22.0),
                                              json_get_str(in, "key_path", ""));
        } else if (!strcmp(func, "build_ssh_command")) {
            out = w19_ssh_5f7591_build_ssh_command(
                json_get_str(in, "control_socket", ""), json_get_str(in, "user", "root"),
                json_get_str(in, "host", "example"), (long long)json_get_num(in, "port", 22.0),
                json_get_str(in, "key_path", ""), json_obj_get(in, "send_env"),
                json_obj_get(in, "extra_args"), jbool(in, "multiplex", 0));
        } else if (!strcmp(func, "ensure_ssh_available")) {
            const char *r = w19_ssh_5f7591_ensure_ssh_available(jbool(in, "ssh_ok", 1), jbool(in, "scp_ok", 1));
            out = r ? json_string(r) : json_null();
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
