/*
 * t_auto_title_config.c — faithful oracle for agent/title_generator.py:
 * _auto_title_enabled branch logic (config-driven truthy collapse).
 * Exercises auto_title_from_config() against a constructed config JSON and
 * compares to Python's _auto_title_enabled() invoked with the same config
 * object (no disk I/O; deterministic). Self-verifying.
 */

#include "title_generator_helpers.h"
#include "truthy.h"            /* ensure is_truthy_value link */
#include "port_config_py_helpers.h" /* config_py_get_nested */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
                       else printf("ok: %s\n", m); } while (0)

/* Build the relevant config slice: {"auxiliary": {"title_generation": {"enabled": <val>}}} */
static json_t *mk_cfg(const char *enabled_json) {
    /* enabled_json is a JSON literal for the enabled field, or NULL to omit. */
    char buf[512];
    if (enabled_json) {
        snprintf(buf, sizeof buf,
                 "{\"auxiliary\":{\"title_generation\":{\"enabled\":%s}}}",
                 enabled_json);
    } else {
        snprintf(buf, sizeof buf, "{\"auxiliary\":{}}");
    }
    return json_parse(buf, NULL);
}

int main(void) {
    struct { const char *label; const char *enabled; bool want; } cases[] = {
        {"bool true",    "true",   true},
        {"bool false",   "false",  false},
        {"null -> def",  "null",   true},
        {"string yes",   "\"yes\"", true},
        {"string no",    "\"no\"",  false},
        {"string 1",     "\"1\"",   true},
        {"string garbage", "\"maybe\"", false},   /* non-member -> false */
        {"number 1",     "1",      true},
        {"number 0",     "0",      false},
        {"number 5",     "5",      true},
        {"omit -> def",  NULL,     true},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        json_t *cfg = mk_cfg(cases[i].enabled);
        bool got = auto_title_from_config(cfg, true);
        CHECK(got == cases[i].want, cases[i].label);
        json_free(cfg);
    }

    /* default false variant (defensive): omit -> default false. */
    json_t *c = mk_cfg(NULL);
    CHECK(auto_title_from_config(c, false) == false, "omit -> default false");
    json_free(c);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
