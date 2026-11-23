/*
 * bitwarden.c — real C port of upstream Hermes agent/vault_backends/bitwarden.py constant tables.
 * PoP: cn_ead89e817f_constants @ agent/vault_backends/bitwarden.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_ead89e817f.h"

json_t *cn_ead89e817f_constants(void) {
    json_t *out = json_object();
    json_set(out, "_TIMEOUT", json_number(30));
    json_set(out, "_ENV_KEEP", ({ json_t *__a = json_array();
        json_append(__a, json_string("PATH"));
        json_append(__a, json_string("HOME"));
        json_append(__a, json_string("USERPROFILE"));
        json_append(__a, json_string("APPDATA"));
        json_append(__a, json_string("LOCALAPPDATA"));
        json_append(__a, json_string("SystemRoot"));
        json_append(__a, json_string("TMPDIR"));
        json_append(__a, json_string("TMP"));
        json_append(__a, json_string("TEMP"));
        json_append(__a, json_string("XDG_CONFIG_HOME"));
        json_append(__a, json_string("BITWARDENCLI_APPDATA_DIR"));
      __a; }));
    return out;
}
