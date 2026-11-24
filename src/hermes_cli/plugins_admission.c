/*
 * plugins_admission.c - C home for upstream Hermes hermes_cli/plugins_admission.py.
 * PoP: w16_pluginsadm_058c05_dependency_conflict_message @ hermes_cli/plugins_admission.py:DependencyConflict.__init__
 *
 * Faithful port of the DependencyConflict message construction: the
 * plugin-vs-generic "who" prefix followed by the fixed resolver sentence.
 * PM/venv side effects (sync_venv, candidate_members) stay in Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_pluginsadm_058c05.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

/* Python str(v) for the scalar types an f-string may interpolate. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        case JSON_ARRAY:  return dupstr("[]");
        case JSON_OBJECT: return dupstr("{}");
        default:          return dupstr("");
    }
}

struct json_t *w16_pluginsadm_058c05_dependency_conflict_message(const struct json_t *cause,
                                                                const struct json_t *plugin) {
    char *who_prefix = truthy(plugin) ? dupstr("Plugin '") : dupstr("");
    char *who = truthy(plugin) ? py_str(plugin) : dupstr("The plugin selection");
    char *pname = truthy(plugin) ? py_str(plugin) : dupstr("");
    char *c = py_str(cause);

    size_t n = strlen(who_prefix) + strlen(who) + strlen(pname) + strlen(c) + 256;
    char *out = (char *)malloc(n);
    if (!out) { free(who_prefix); free(who); free(pname); free(c); return json_string(""); }
    if (truthy(plugin))
        snprintf(out, n, "Plugin '%s' conflicts with the dependencies pinned by Hermes core or an "
                         "enabled plugin, so it was not admitted. Resolver: %s", who, c);
    else
        snprintf(out, n, "The plugin selection conflicts with the dependencies pinned by Hermes core "
                         "or an enabled plugin, so it was not admitted. Resolver: %s", c);
    json_t *res = json_string(out);
    free(out); free(who_prefix); free(who); free(pname); free(c);
    return res;
}
