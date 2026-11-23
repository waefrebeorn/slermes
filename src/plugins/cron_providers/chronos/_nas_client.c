/*
 * _nas_client.c — real C port of upstream Hermes plugins/cron_providers/chronos/_nas_client.py constant tables.
 * PoP: cn_9c9d9ed12b_constants @ plugins/cron_providers/chronos/_nas_client.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_9c9d9ed12b.h"

json_t *cn_9c9d9ed12b_constants(void) {
    json_t *out = json_object();
    json_set(out, "_PROVISION_PATH", json_string("/api/agent-cron/provision"));
    json_set(out, "_CANCEL_PATH", json_string("/api/agent-cron/cancel"));
    json_set(out, "_LIST_PATH", json_string("/api/agent-cron/list"));
    return out;
}
