/*
 * hermes_state_registry.c — faithful C11 port of upstream Hermes
 * hermes_state_registry.py (gate w5_hermesstate_732fec).
 *
 * PoP: w5_hermesstate_732fec_release @ hermes_state_registry.py:release
 * PoP: w5_hermesstate_732fec_acquire_variant @ hermes_state_registry.py:acquire
 *
 * The module is a deprecation shim; all observable behaviour is the class it
 * lends, the one-time warning decision and the close-on-release contract. The
 * SessionDB objects themselves are side-effect seams injected by the fixture.
 */
#include "noc_w5_hermesstate_732fec.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct json_t *w5_hermesstate_732fec_release(int is_none) {
    struct json_t *out = json_object();
    json_set(out, "result", json_bool(is_none ? 0 : 1));
    json_set(out, "closed", json_bool(is_none ? 0 : 1));
    return out;
}

struct json_t *w5_hermesstate_732fec_acquire_variant(int first_warning_present,
                                                     int first_warning_result,
                                                     const char *removal_release,
                                                     const char *migration_guide) {
    struct json_t *out = json_object();
    if (!first_warning_present) {
        json_set(out, "db_class", json_string("SessionDB"));
        json_set(out, "warn", json_bool(0));
        json_set(out, "msg", json_null());
        return out;
    }
    json_set(out, "db_class", json_string("_SessionDB"));
    if (!first_warning_result) {
        json_set(out, "warn", json_bool(0));
        json_set(out, "msg", json_null());
        return out;
    }
    if (!removal_release) removal_release = "";
    if (!migration_guide) migration_guide = "";
    {
        char msg[8192];
        int rc = snprintf(msg, sizeof(msg),
            "hermes_state_registry.acquire is deprecated and is removed in %s; use "
            "persistence_assembly.open_persistence(profile_scope()) instead (%s).",
            removal_release, migration_guide);
        if (rc < 0 || (size_t)rc >= sizeof(msg)) {
            json_set(out, "warn", json_bool(1));
            json_set(out, "msg", json_null());
            return out;
        }
        json_set(out, "warn", json_bool(1));
        json_set(out, "msg", json_string(msg));
    }
    return out;
}
