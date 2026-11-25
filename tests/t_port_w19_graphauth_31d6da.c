#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_graphauth_31d6da.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int req_has(json_t *in, const char *key) {
    return json_obj_get(in, key) != NULL;
}

static int req_int(json_t *in, const char *key, int def) {
    return req_has(in, key) ? (int)json_get_num(in, key, def) : def;
}

static int req_bool(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return def;
}

/* Credentials for inspect_health: the request carries them as top-level fields. */
static json_t *req_credentials(json_t *in) {
    static const char *const KEYS[] = {"tenant_id", "client_id", "client_secret", "scope", "authority_url", NULL};
    json_t *creds = json_object();
    int i;
    for (i = 0; KEYS[i]; i++) {
        json_t *v = json_obj_get(in, KEYS[i]);
        if (v) json_set(creds, KEYS[i], json_copy(v));
    }
    return creds;
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
        if (!strcmp(func, "format_error")) {
            char *s = w19_graphauth_31d6da_format_graph_error(json_obj_get(in, "error"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "from_env")) {
            out = w19_graphauth_31d6da_credentials_from_env(json_obj_get(in, "env_map"),
                                                            req_bool(in, "required", 1));
        } else if (!strcmp(func, "token_url")) {
            char *s = w19_graphauth_31d6da_token_url(json_get_str(in, "authority_url", ""),
                                                     json_get_str(in, "tenant_id", ""));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "is_expired")) {
            out = json_bool(w19_graphauth_31d6da_token_is_expired(
                json_get_num(in, "expires_at", 0.0), json_get_num(in, "now", 0.0),
                req_int(in, "skew_seconds", 120)) ? 1 : 0);
        } else if (!strcmp(func, "expires_in")) {
            out = w19_graphauth_31d6da_token_expires_in(json_get_num(in, "expires_at", 0.0),
                                                        json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "fresh_cached")) {
            out = json_bool(w19_graphauth_31d6da_fresh_cached(
                req_bool(in, "has_cached", 0), json_get_num(in, "expires_at", 0.0),
                json_get_num(in, "now", 0.0), req_int(in, "skew_seconds", 120)) ? 1 : 0);
        } else if (!strcmp(func, "inspect_health")) {
            json_t *creds = req_credentials(in);
            out = w19_graphauth_31d6da_inspect_health(creds, req_bool(in, "has_cached", 0),
                                                      json_get_num(in, "expires_at", 0.0),
                                                      json_get_num(in, "now", 0.0),
                                                      req_int(in, "skew_seconds", 120));
            json_free(creds);
        } else if (!strcmp(func, "error_detail")) {
            char *s = w19_graphauth_31d6da_extract_error_detail(req_bool(in, "is_json", 1),
                                                                json_obj_get(in, "payload"),
                                                                json_get_str(in, "text", ""));
            out = s ? json_string(s) : json_null();
            free(s);
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
