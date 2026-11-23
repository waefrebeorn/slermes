/*
 * base_wait.c — real C port of upstream Hermes tools/environments/base_wait.py constant tables.
 * PoP: cn_19ed142069_constants @ tools/environments/base_wait.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_19ed142069.h"

json_t *cn_19ed142069_constants(void) {
    json_t *out = json_object();
    json_set(out, "_FMT", ({ json_t *__o = json_object();
        json_set(__o, "ENTER", json_string("timeout=%ss activity_cb=%s initial_interrupt=%s"));
        json_set(__o, "EXCEPTION_EXIT", json_string("iter=%d elapsed=%.1fs — killing subprocess group before re-raise"));
        json_set(__o, "EXIT (natural)", json_string("iter=%d elapsed=%.1fs returncode=%s"));
        json_set(__o, "HEARTBEAT", json_string("iter=%d elapsed=%.0fs interrupt=%s activity_cb=%s%s"));
        json_set(__o, "INTERRUPT DETECTED", json_string("iter=%d elapsed=%.1fs — killing process group"));
        json_set(__o, "TIMEOUT", json_string("iter=%d timeout=%ss"));
      __o; }));
    return out;
}
