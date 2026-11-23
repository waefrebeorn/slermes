/*
 * approval_config_set.c — real C port of upstream Hermes tools/approval_config_set.py constant tables.
 * PoP: cn_052b37daf8_constants @ tools/approval_config_set.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_052b37daf8.h"

json_t *cn_052b37daf8_constants(void) {
    json_t *out = json_object();
    json_set(out, "_HERMES_TOP_LEVEL_VALUE_FLAGS", json_string("(?:--profile|-p|--config|--model|-m|--provider|--reasoning|--toolsets|-t|--resume|-r|--skills|-s|--oneshot|-z|--usage-file|--in|--cpu-profile)"));
    json_set(out, "_HERMES_PROFILE_OPTION", json_string("(?:(?:-p|--profile)[ \\t]+[^\\s]++|--profile=[^\\s]++)"));
    json_set(out, "_SHELL_WORD", json_string("(?:[^\\s'\"]++|'[^']*+'|\"[^\"]*+\")*+"));
    return out;
}
