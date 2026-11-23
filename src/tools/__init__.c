/*
 * __init__.c — C home for upstream Hermes tools/__init__.py
 * Gate: w5_init_e47994
 *
 * PoP: w5_init_e47994_check_file_requirements @ tools/__init__.py:check_file_requirements
 * PoP: w5_init_e47994_all @ tools/__init__.py:__all__
 *
 * The package namespace is deliberately side-effect free: importing it must not
 * load the tool stack. Its only callable is a one-hop delegation to the terminal
 * backend's requirement check; the injected value stands in for that backend.
 */
#include "noc_w5_init_e47994.h"
#include "hermes_json.h"

int w5_init_e47994_check_file_requirements(int delegated) {
    return delegated ? 1 : 0;
}

struct json_t *w5_init_e47994_all(void) {
    struct json_t *a = json_array();
    json_append(a, json_string("check_file_requirements"));
    return a;
}
