/*
 * source_stamp.c — C home for upstream Hermes hermes_cli/source_stamp.py.
 *
 * PoP: w9_sourcestamp_c21b1e_write_source_stamp @ hermes_cli/source_stamp.py:write_source_stamp
 * PoP: w9_sourcestamp_c21b1e_bootstrap_receipt_key @ hermes_cli/source_stamp.py:BOOTSTRAP_RECEIPT
 *
 * The impure layers of the Python function (the git provenance walk, the
 * wall-clock timestamp, the atomic file publication and the bootstrap-receipt
 * refresh) are injected as data by the fixture.  What is ported here is the
 * exact stamp-dict construction and its no-identity branch.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#include "noc_w9_sourcestamp_c21b1e.h"

static const char *const SOURCE_BOOTSTRAP_RECEIPT = ".hermes-bootstrap-complete";

/* Python: value or None.  Empty fixture string is the None sentinel. */
static json_t *str_or_null(const char *s) {
    return (s && *s) ? json_string(s) : json_null();
}

static json_t *int_or_null(const char *s) {
    if (!s || !*s) return json_null();
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s || (end && *end)) return json_null();
    return json_number((double)v);
}

struct json_t *w9_sourcestamp_c21b1e_write_source_stamp(
    const char *commit,
    const char *commit_date,
    const char *branch,
    const char *base_version,
    const char *derived_version,
    const char *distance,
    int dirty,
    const char *built_at,
    int adopted) {
    /* info.commit is None -> unlink old stamp, reset cache, return None. */
    if (!commit || !*commit) return json_null();

    json_t *stamp = json_object();
    json_set(stamp, "schemaVersion", json_number(2));
    json_set(stamp, "commit", json_string(commit));
    json_set(stamp, "commitDate", int_or_null(commit_date));
    json_set(stamp, "branch", str_or_null(branch));
    json_set(stamp, "builtAt", json_string(built_at ? built_at : ""));
    json_set(stamp, "dirty", json_bool(dirty ? true : false));
    json_set(stamp, "source", json_string("git"));
    json_set(stamp, "distribution", json_null());
    json_set(stamp, "updateMechanism", json_string("self"));
    json_set(stamp, "baseVersion", json_string(base_version ? base_version : ""));
    json_set(stamp, "displayVersion", json_string(derived_version ? derived_version : ""));
    json_set(stamp, "distance", int_or_null(distance));
    json_set(stamp, "payload", json_string("bootstrap"));
    json_set(stamp, "tag", json_null());
    if (adopted)
        json_set(stamp, "adoptedAt", json_string(built_at ? built_at : ""));
    return stamp;
}

struct json_t *w9_sourcestamp_c21b1e_bootstrap_receipt_key(void) {
    return json_string(SOURCE_BOOTSTRAP_RECEIPT);
}

void hermes_cli_source_stamp_port_pending(void) { (void)0; }
