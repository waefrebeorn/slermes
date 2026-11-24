/*
 * session_worker_child.c — C port of upstream Hermes gateway/session_worker_child.py.
 *
 * PoP: w16_sessionwor_8fa979_limit @ gateway/session_worker_child.py:_limit
 * PoP: w16_sessionwor_8fa979_root_execution_id @ gateway/session_worker_child.py:delegation_limits
 * PoP: w16_sessionwor_8fa979_is_child @ gateway/session_worker_child.py:delegation_limits
 *
 * CHILD_SEPARATOR is "/child:" (persistence/workers/vocabulary.py), read from the
 * live constant by the oracle. int() below follows Python semantics on the JSON
 * fixture domain (numbers truncate; strings must parse wholly; bool -> 1/0).
 */
#include "noc_w16_sessionwor_8fa979.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CHILD_SEPARATOR "/child:"

static int py_int_of(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        errno = 0;
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (end == s || errno == ERANGE) return 0;
        while (isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = n;
        return 1;
    }
    return 0;
}

static long long default_for(const char *key) {
    return (!strcmp(key, "max_concurrent_children")) ? 10 : 1;
}

long long w16_sessionwor_8fa979_limit(const json_t *section, const char *key) {
    long long value;
    if (section && section->type == JSON_OBJECT &&
        py_int_of(json_obj_get(section, key), &value))
        return value > 1 ? value : 1;         /* max(1, int(...)) */
    return default_for(key);
}

json_t *w16_sessionwor_8fa979_root_execution_id(const char *execution_id) {
    const char *s = execution_id ? execution_id : "";
    const char *sep = strstr(s, CHILD_SEPARATOR);
    if (!sep) return json_string(s);
    size_t n = (size_t)(sep - s);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return json_string("");
    memcpy(buf, s, n);
    buf[n] = '\0';
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

int w16_sessionwor_8fa979_is_child(const char *execution_id) {
    return (execution_id && strstr(execution_id, CHILD_SEPARATOR)) ? 1 : 0;
}

