/*
 * __init__.c — faithful C11 port of hermes_cli/__init__.py.
 *
 * PoP: w7_init_51a22b_doc           @ hermes_cli/__init__.py:__doc__
 * PoP: w7_init_51a22b_release_date  @ hermes_cli/__init__.py:__release_date__
 * PoP: w7_init_51a22b_getattr       @ hermes_cli/__init__.py:__getattr__
 * PoP: w7_init_51a22b_public_names  @ hermes_cli/__init__.py:<module>
 * PoP: w7_init_51a22b_has_symbol    @ hermes_cli/__init__.py:<module>
 *
 * The package shim binds a release date, docstring, a lazy __getattr__ for the
 * old-updater '__version__' surface, and a process-local UTF-8 repair (a side
 * effect outside the JSON contract). The install stamp read by __getattr__ is
 * injected from the fixture to keep the port pure and deterministic.
 */
#include "noc_w7_init_51a22b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const NAMES[] = {"_ensure_utf8", "_stdio_repaired", "sys"};
static const size_t NNAMES = sizeof(NAMES) / sizeof(NAMES[0]);

const char *w7_init_51a22b_doc(void) {
    return "Hermes CLI - Unified command-line interface for Hermes Agent.";
}

const char *w7_init_51a22b_release_date(void) {
    return "2026.9.24";
}

struct json_t *w7_init_51a22b_getattr(const char *name, const struct json_t *base_version) {
    if (!name || strcmp(name, "__version__")) return json_null(); /* AttributeError */
    /* return str(read_install_stamp(...).get('baseVersion') or '0.0.0') */
    if (!base_version || base_version->type == JSON_NULL) return json_string("0.0.0");
    if (base_version->type == JSON_STRING) {
        const char *s = base_version->str_val ? base_version->str_val : "";
        return json_string(*s ? s : "0.0.0");
    }
    if (base_version->type == JSON_BOOL)
        return json_string(base_version->bool_val ? "True" : "0.0.0");
    if (base_version->type == JSON_NUMBER) {
        if (base_version->num_val == 0.0) return json_string("0.0.0");
        char buf[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(buf, sizeof(buf), "%.*g", prec, base_version->num_val);
            if (strtod(buf, NULL) == base_version->num_val) break;
        }
        return json_string(buf);
    }
    return json_string("0.0.0");
}

struct json_t *w7_init_51a22b_public_names(void) {
    struct json_t *out = json_array();
    for (size_t i = 0; i < NNAMES; i++) json_append(out, json_string(NAMES[i]));
    return out;
}

int w7_init_51a22b_has_symbol(const char *name) {
    if (!name) return 0;
    for (size_t i = 0; i < NNAMES; i++) if (!strcmp(name, NAMES[i])) return 1;
    return 0;
}
