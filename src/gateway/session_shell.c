/*
 * session_shell.c -- faithful C11 port of the pure decision surface in
 *                    gateway/session_shell.py.
 *
 * PoP: w8_sessionshel_fe6651_timeout_seconds @ gateway/session_shell.py:TIMEOUT_SECONDS
 * PoP: w8_sessionshel_fe6651_local_backends  @ gateway/session_shell.py:_LOCAL_BACKENDS
 * PoP: w8_sessionshel_fe6651_handler_names   @ gateway/session_shell.py:handlers
 * PoP: w8_sessionshel_fe6651_params_valid    @ gateway/session_shell.py:shell_exec
 * PoP: w8_sessionshel_fe6651_backend_is_local @ gateway/session_shell.py:shell_exec
 */
#include "noc_w8_sessionshel_fe6651.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const ALLOWED[] = {"session_id", "profile", "command"};

static int w8sh_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *w8sh_strip_lower(const char *s) {
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && w8sh_isspace((unsigned char)s[a])) a++;
    while (b > a && w8sh_isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    for (size_t i = 0; i < b - a; i++) out[i] = (char)tolower((unsigned char)s[a + i]);
    out[b - a] = '\0';
    return out;
}

struct json_t *w8_sessionshel_fe6651_timeout_seconds(void) {
    return json_number(30.0);
}

struct json_t *w8_sessionshel_fe6651_local_backends(void) {
    /* sorted(frozenset({'', 'local'})) -> ['', 'local'] */
    json_t *out = json_array();
    json_append(out, json_string(""));
    json_append(out, json_string("local"));
    return out;
}

struct json_t *w8_sessionshel_fe6651_handler_names(void) {
    /* handlers(connection) -> {'shell.exec': partial(...)} */
    json_t *out = json_array();
    json_append(out, json_string("shell.exec"));
    return out;
}

struct json_t *w8_sessionshel_fe6651_params_valid(const struct json_t *params,
        const struct json_t *session_id, const struct json_t *command) {
    int invalid = 0;
    if (!params || params->type != JSON_OBJECT) {
        invalid = 1;
    } else {
        size_t n = json_object_size(params);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(params, i);
            int ok = 0;
            for (int j = 0; j < 3; j++) if (k && !strcmp(k, ALLOWED[j])) { ok = 1; break; }
            if (!ok) { invalid = 1; break; }
        }
    }
    if (!session_id || session_id->type != JSON_STRING || !session_id->str_val || !session_id->str_val[0])
        invalid = 1;
    if (!command || command->type != JSON_STRING || !command->str_val) {
        invalid = 1;
    } else {
        char *s = w8sh_strip_lower(command->str_val);
        if (!s[0]) invalid = 1;
        free(s);
    }
    return json_bool(!invalid);
}

struct json_t *w8_sessionshel_fe6651_backend_is_local(const struct json_t *terminal_env) {
    const char *value = "";
    if (terminal_env && terminal_env->type == JSON_STRING && terminal_env->str_val)
        value = terminal_env->str_val;
    char *norm = w8sh_strip_lower(value);
    int local = (!strcmp(norm, "") || !strcmp(norm, "local"));
    free(norm);
    return json_bool(local);
}
