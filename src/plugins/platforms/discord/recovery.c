/*
 * recovery.c — real C port of upstream Hermes plugins/platforms/discord/recovery.py constant tables.
 * PoP: cn_3145e44260_constants @ plugins/platforms/discord/recovery.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_3145e44260.h"

json_t *cn_3145e44260_constants(void) {
    json_t *out = json_object();
    json_set(out, "_DB_FILENAME", json_string("discord_message_recovery.db"));
    json_set(out, "_RETENTION_DAYS", json_number(30));
    return out;
}
