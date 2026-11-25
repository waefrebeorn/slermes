/*
 * t_port_w6_acpclient_6f530e.c -- JSON-lines oracle harness for the C11 port
 * of agent/acp_client.py (noc_w6_acpclient_6f530e.h). Reads one fixture object
 * per stdin line, dispatches on "func", emits {"func","out",...echoed inputs}.
 */
#include "noc_w6_acpclient_6f530e.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* raw JSON text of a node, or "null" when absent */
static char *node_text(json_t *n) {
    if (!n) return strdup("null");
    char *s = json_serialize(n);
    return s ? s : strdup("null");
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "result")) {
            char *idj = node_text(json_obj_get(in, "id"));
            char *resj = node_text(json_obj_get(in, "result"));
            out = w6_acpclient_6f530e_jsonrpc_result(idj, resj);
            free(idj); free(resj);
        } else if (!strcmp(func, "error")) {
            char *idj = node_text(json_obj_get(in, "id"));
            int code = (int)json_get_num(in, "code", 0);
            const char *msg = json_get_str(in, "message", "");
            out = w6_acpclient_6f530e_jsonrpc_error(idj, code, msg);
            free(idj);
        } else if (!strcmp(func, "ids")) {
            char *ent = node_text(json_obj_get(in, "entries"));
            const char *key = json_get_str(in, "key", "");
            out = w6_acpclient_6f530e_enabled_id_list(ent, key);
            free(ent);
        } else if (!strcmp(func, "config_option")) {
            char *sess = node_text(json_obj_get(in, "session"));
            out = w6_acpclient_6f530e_model_config_option(sess);
            free(sess);
        } else if (!strcmp(func, "session_ids")) {
            char *sess = node_text(json_obj_get(in, "session"));
            out = w6_acpclient_6f530e_session_model_ids(sess);
            free(sess);
        } else if (!strcmp(func, "legacy_ids")) {
            char *sess = node_text(json_obj_get(in, "session"));
            out = w6_acpclient_6f530e_legacy_session_model_ids(sess);
            free(sess);
        } else if (!strcmp(func, "selection")) {
            char *sess = node_text(json_obj_get(in, "session"));
            const char *model = json_get_str(in, "model", "");
            const char *ph = json_get_str(in, "ph", "");
            out = w6_acpclient_6f530e_model_selection_request(sess, model, ph);
            free(sess);
        } else if (!strcmp(func, "timeout")) {
            int isnum = json_get_bool(in, "is_number", 0);
            double number = json_get_num(in, "number", 0);
            w6_acpclient_6f530e_timeout_t o;
            memset(&o, 0, sizeof(o));
            o.has_read = json_get_bool(in, "has_read", 0);    o.read = json_get_num(in, "read", 0);
            o.has_write = json_get_bool(in, "has_write", 0);  o.write = json_get_num(in, "write", 0);
            o.has_connect = json_get_bool(in, "has_connect", 0); o.connect = json_get_num(in, "connect", 0);
            o.has_pool = json_get_bool(in, "has_pool", 0);    o.pool = json_get_num(in, "pool", 0);
            o.has_timeout = json_get_bool(in, "has_timeout", 0); o.timeout = json_get_num(in, "timeout", 0);
            double t = w6_acpclient_6f530e_effective_timeout(isnum, number, &o);
            out = json_number(t);
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
