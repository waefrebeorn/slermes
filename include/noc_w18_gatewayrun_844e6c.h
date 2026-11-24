#ifndef NOC_W18_GATEWAYRUN_844E6C_H
#define NOC_W18_GATEWAYRUN_844E6C_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_runtime_cli.py
 * (gate w18_gatewayrun_844e6c). NULL reason_code mirrors Python None;
 * `code_sha` is the injected get_code_identity().get("sha"). */

json_t *w18_gatewayrun_844e6c_ensure_exit_code(const char *reason_code, const char *state);
json_t *w18_gatewayrun_844e6c_verdict_payload(const json_t *result, const char *code_sha);
json_t *w18_gatewayrun_844e6c_invalid_invocation(void);

#endif /* NOC_W18_GATEWAYRUN_844E6C_H */
