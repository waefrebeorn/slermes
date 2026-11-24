/*
 * run_plugin_injection.c — C port of upstream Hermes gateway/run_plugin_injection.py.
 *
 * PoP: w16_runplugin_bf0c8c_plugin_admission @ gateway/run_plugin_injection.py:_plugin_admission
 *
 * The pure admission predicate is gated; the scheduler/dispatch/authority methods are
 * asyncio/DB-bound and stay outside this gate.
 */
#include "noc_w16_runplugin_bf0c8c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
/* Python str(v) for the scalar values str() is applied to here. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

json_t *w16_runplugin_bf0c8c_plugin_admission(const json_t *row) {
    int admit = 0;
    if (json_is_object(row)) {
        const json_t *rid = json_obj_get(row, "request_id");
        if (json_is_string(rid) && rid->str_val && rid->str_val[0] == '[') {
            char *pid = py_str(json_obj_get(row, "principal_id"));
            int automation = strncmp(pid, "automation:", 11) == 0;
            free(pid);
            if (automation) {
                /* with contextlib.suppress(ValueError): identity = json.loads(request_id)
                 * json_parse is strict about trailing bytes, matching json.loads. */
                json_t *identity = json_parse(rid->str_val, NULL);
                if (json_is_array(identity) && json_len(identity) >= 1) {
                    const json_t *first = json_get(identity, 0);
                    if (json_is_string(first) && first->str_val && !strcmp(first->str_val, "plugin"))
                        admit = 1;  /* identity[:1] == ["plugin"] */
                }
                json_free(identity);
            }
        }
    }
    return json_bool(admit);
}
