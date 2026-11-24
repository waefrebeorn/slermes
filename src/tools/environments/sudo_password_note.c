/*
 * sudo_password_note.c - C11 port of upstream
 * tools/environments/sudo_password_note.py (gate w24_sudopasswo_63ce58).
 *
 * PoP: w24_sudopasswo_63ce58_backend_label @ tools/environments/sudo_password_note.py:backend_label
 * PoP: w24_sudopasswo_63ce58_note @ tools/environments/sudo_password_note.py:note_sudo_password_sent
 * PoP: w24_sudopasswo_63ce58_constants @ tools/environments/sudo_password_note.py:_CAMEL_BOUNDARY
 *
 * Ported: the whole decision.  ``_noted_backend_types`` + ``_noted_lock`` and
 * the ``logging`` call are seams -- this file says whether the FIRST send on a
 * backend class logs and what the message reads; the caller keeps the set and
 * emits the record.  The password never leaves the argument: only whether it
 * is None, and whether it is newline-only, is consulted.
 *
 * Python's ``str.strip()`` (all whitespace) and ``str.strip("\n")`` (newlines
 * only) are both reproduced, as is the ``(?<=[a-z0-9])(?=[A-Z])`` camel-case
 * split that turns a class name into a backend label.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sudopasswo_63ce58.h"

#define WARNING_PREFIX \
    "The sudo password is sent to the "
#define WARNING_SUFFIX \
    " terminal backend, where programs running in it can read it. Use a password " \
    "that is only valid there, or passwordless sudo in that environment's image, " \
    "rather than the password of the machine running Hermes."

static const char *as_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* type(v).__name__ for a JSON value (int/float split is integral-vs-fractional
 * because JSON does not carry it). */
static const char *py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python truthiness. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* _CAMEL_BOUNDARY.sub("_", name).lower() */
static char *snake_lower(const char *name) {
    size_t n = strlen(name), i, o = 0;
    char *out = (char *)malloc(2 * n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (i > 0) {
            unsigned char p = (unsigned char)name[i - 1];
            int prev_ok = (p >= 'a' && p <= 'z') || (p >= '0' && p <= '9');
            int cur_ok = (c >= 'A' && c <= 'Z');
            if (prev_ok && cur_ok) out[o++] = '_';
        }
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        out[o++] = (char)c;
    }
    out[o] = '\0';
    return out;
}

json_t *w24_sudopasswo_63ce58_backend_label(const json_t *env) {
    const json_t *stamped_node = NULL;
    const char *class_name = NULL;
    char *name = NULL, *label = NULL;
    json_t *out;

    if (env && env->type == JSON_OBJECT) {
        stamped_node = json_obj_get(env, "backend_name");
        class_name = as_str(json_obj_get(env, "class_name"));
        if (!class_name) class_name = py_type_name(env);
    } else {
        class_name = py_type_name(env);
    }

    /* A plugin backend's registered name, when one is stamped and non-blank. */
    if (stamped_node && stamped_node->type == JSON_STRING) {
        const char *s = stamped_node->str_val ? stamped_node->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && py_space((unsigned char)s[a])) a++;
        while (b > a && py_space((unsigned char)s[b - 1])) b--;
        if (b > a) {
            char *trimmed = (char *)malloc(b - a + 1);
            if (!trimmed) return json_null();
            memcpy(trimmed, s + a, b - a);
            trimmed[b - a] = '\0';
            out = json_string(trimmed);
            free(trimmed);
            return out;
        }
    }

    name = (char *)malloc(strlen(class_name) + 1);
    if (!name) return json_null();
    strcpy(name, class_name);
    {
        size_t nl = strlen(name);
        if (nl > 11 && strcmp(name + nl - 11, "Environment") == 0 && strcmp(name, "Environment") != 0)
            name[nl - 11] = '\0';
    }
    label = snake_lower(name);
    free(name);
    out = json_string(label ? label : "");
    free(label);
    return out;
}

json_t *w24_sudopasswo_63ce58_note(const json_t *env, const json_t *sudo_stdin) {
    const char *pw;
    size_t a, b;
    json_t *label, *out;
    char *msg;
    size_t need;
    int is_local = 0;

    if (sudo_stdin && sudo_stdin->type != JSON_NULL && sudo_stdin->type != JSON_STRING)
        return json_string("ERROR:attribute");   /* upstream: .strip on a non-str raises */
    pw = as_str(sudo_stdin);

    if (env && env->type == JSON_OBJECT) is_local = truthy(json_obj_get(env, "is_local"));

    if (!pw) {                                   /* sudo_stdin is None */
        out = json_object();
        json_set(out, "logged", json_bool(0));
        json_set(out, "message", json_null());
        return out;
    }
    a = 0;
    b = strlen(pw);
    while (a < b && pw[a] == '\n') a++;
    while (b > a && pw[b - 1] == '\n') b--;
    if (b == a || is_local) {
        out = json_object();
        json_set(out, "logged", json_bool(0));
        json_set(out, "message", json_null());
        return out;
    }

    label = w24_sudopasswo_63ce58_backend_label(env);
    need = strlen(WARNING_PREFIX) + strlen(as_str(label) ? as_str(label) : "") +
           strlen(WARNING_SUFFIX) + 1;
    msg = (char *)malloc(need);
    if (!msg) { json_free(label); return json_null(); }
    snprintf(msg, need, "%s%s%s", WARNING_PREFIX,
             as_str(label) ? as_str(label) : "", WARNING_SUFFIX);
    out = json_object();
    json_set(out, "logged", json_bool(1));
    json_set(out, "message", json_string(msg));
    free(msg);
    json_free(label);
    return out;
}

json_t *w24_sudopasswo_63ce58_constants(void) {
    json_t *o = json_object();
    json_set(o, "camel_boundary", json_string("(?<=[a-z0-9])(?=[A-Z])"));
    return o;
}
