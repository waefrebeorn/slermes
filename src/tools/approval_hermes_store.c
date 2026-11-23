/*
 * approval_hermes_store.c — real C port of upstream Hermes tools/approval_hermes_store.py constant tables.
 * PoP: cn_97ac1634a4_constants @ tools/approval_hermes_store.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_97ac1634a4.h"

json_t *cn_97ac1634a4_constants(void) {
    json_t *out = json_object();
    json_set(out, "_HERMES_HOME_ROOT", json_string("(?:~/\\.hermes|(?:\\$home|\\$\\{home\\})/\\.hermes|(?:\\$hermes_home|\\$\\{hermes_home\\}))"));
    json_set(out, "_PATH_WORD_END", json_string("[\"\\']?(?=[\\s\"\\'`;|&<>()]|$)"));
    return out;
}
