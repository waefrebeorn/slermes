/*
 * main_dep_hints.c — C home for upstream Hermes hermes_cli/main_dep_hints.py.
 *
 * PoP: w12_maindephi_e98fd2_missing_optional_deps_message @ hermes_cli/main_dep_hints.py:missing_optional_deps_message
 * PoP: w12_maindephi_e98fd2_smart_app_control_block_message @ hermes_cli/main_dep_hints.py:smart_app_control_block_message
 */
#include "noc_w12_maindephi_e98fd2.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str() coercion for the scalar JSON values fed to these helpers. */
static char *py_str(const json_t *v) {
    if (!v) return strdup("None");
    switch (v->type) {
        case JSON_NULL:   return strdup("None");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double n = v->num_val;
            if (n == (double)(long long)n && n > -9e15 && n < 9e15)
                snprintf(buf, sizeof(buf), "%lld", (long long)n);
            else
                snprintf(buf, sizeof(buf), "%g", n);
            return strdup(buf);
        }
        default: {
            char *s = json_dumps(v, 0);
            return s ? s : strdup("");
        }
    }
}

char *w12_maindephi_e98fd2_missing_optional_deps_message(const char *surface,
                                                          const char *what,
                                                          const char *extra) {
    (void)extra; /* the live Python signature has this parameter but never reads it */
    char buf[8192];
    snprintf(buf, sizeof(buf),
        "The %s can't start: %s are missing from this install.\n"
        "Run `hermes pm install` to prepare the declared dependencies.\n"
        "If an installed dependency is damaged, run `hermes pm repair`, then restart Hermes.",
        surface ? surface : "", what ? what : "");
    return strdup(buf);
}

char *w12_maindephi_e98fd2_smart_app_control_block_message(const struct json_t *error) {
    char *message = py_str(error);
    if (!message) message = strdup("");
    if (!strstr(message, "DLL load failed") || !strstr(message, "_ssl")) {
        free(message);
        return NULL;
    }
    char buf[16384];
    size_t n = 0;
#define ADD(...) n += (size_t)snprintf(buf + n, sizeof(buf) - n, __VA_ARGS__)
    ADD("\xE2\x9C\x97 The embedded Python runtime is blocked by Windows security policy."); ADD("\n");
    ADD(""); ADD("\n");
    ADD("Root cause: Python's SSL module (_ssl) could not be loaded:"); ADD("\n");
    ADD("  %s", message); ADD("\n");
    ADD(""); ADD("\n");
    ADD("This happens when Windows Smart App Control or an Application Control"); ADD("\n");
    ADD("policy blocks the embedded Python runtime that ships with Hermes Desktop."); ADD("\n");
    ADD("A repair / reinstall loop cannot fix this \xE2\x80\x94 the packages are not missing,"); ADD("\n");
    ADD("the runtime's DLL is being blocked."); ADD("\n");
    ADD(""); ADD("\n");
    ADD("Recovery options:"); ADD("\n");
    ADD("  1. Use a trusted system Python installation instead of the embedded"); ADD("\n");
    ADD("     runtime (if your organization allows it)."); ADD("\n");
    ADD("  2. Ask your IT administrator for an exemption for the Hermes Desktop"); ADD("\n");
    ADD("     application (Smart App Control / Application Control)."); ADD("\n");
    ADD("  3. Use the CLI or gateway instead (they use your system Python)."); ADD("\n");
    ADD(""); ADD("\n");
    ADD("See https://aka.ms/smartappcontrol for Windows Smart App Control details.");
#undef ADD
    free(message);
    return strdup(buf);
}
