/*
 * admin.c — real C port of upstream Hermes persistence/core/admin.py constant tables.
 * PoP: cn_6978a463a1_constants @ persistence/core/admin.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_6978a463a1.h"

json_t *cn_6978a463a1_constants(void) {
    json_t *out = json_object();
    json_set(out, "PHYSICAL_ADMIN", json_string("physical_admin"));
    json_set(out, "WRITE_PROBE_SKIPPED", json_string("state.db write-health probe skipped: store is held by a live writer and larger than 1 GB (run 'hermes doctor --fix' to probe it)"));
    json_set(out, "BROWSE_RECONCILE_BUDGET_S", json_number(3));
    json_set(out, "BROWSE_RECONCILE_PURPOSE", json_string("browse_schema_reconcile"));
    return out;
}
