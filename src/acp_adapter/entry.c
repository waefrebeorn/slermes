/*
 * entry.c — C home for upstream Hermes acp_adapter/entry.py.
 * PoP: w6_entry_b16419_parse_args @ acp_adapter/entry.py:_parse_args
 * PoP: w6_entry_b16419_is_benign_probe @ acp_adapter/entry.py:_BENIGN_PROBE_METHODS
 */
#include "noc_w6_entry_b16419.h"
#include "hermes_json.h"
#include <string.h>

static const char *const BENIGN[] = {"ping", "health", "healthcheck"};

int w6_entry_b16419_is_benign_probe(const char *method) {
    if (!method) return 0;
    for (size_t i = 0; i < sizeof(BENIGN) / sizeof(BENIGN[0]); i++)
        if (!strcmp(method, BENIGN[i])) return 1;
    return 0;
}

struct json_t *w6_entry_b16419_parse_args(const struct json_t *argv) {
    json_t *ns = json_object();
    int version = 0, check = 0, setup = 0, setup_browser = 0, assume_yes = 0;
    size_t n = (argv && argv->type == JSON_ARRAY) ? json_len(argv) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *it = json_get(argv, i);
        const char *a = (it && it->type == JSON_STRING) ? it->str_val : NULL;
        if (!a) continue;
        if (!strcmp(a, "--version")) version = 1;
        else if (!strcmp(a, "--check")) check = 1;
        else if (!strcmp(a, "--setup")) setup = 1;
        else if (!strcmp(a, "--setup-browser")) setup_browser = 1;
        else if (!strcmp(a, "--yes") || !strcmp(a, "-y")) assume_yes = 1;
    }
    /* argparse Namespace attribute insertion order matches this order. */
    json_set(ns, "version", json_bool(version));
    json_set(ns, "check", json_bool(check));
    json_set(ns, "setup", json_bool(setup));
    json_set(ns, "setup_browser", json_bool(setup_browser));
    json_set(ns, "assume_yes", json_bool(assume_yes));
    return ns;
}
