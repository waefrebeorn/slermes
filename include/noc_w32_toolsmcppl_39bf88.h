/*
 * noc_w32_toolsmcppl_39bf88.h - C surface for upstream Hermes
 * tui_gateway/contracts/tools_mcp_plugins.py (gate w32_toolsmcppl_39bf88).
 *
 * The upstream module declares no functions: it IS the wire contract for
 * tools / toolsets / reload / skills / learning / MCP servers / plugins --
 * 23 `method(...)` registrations, each pinned to a pydantic Params model
 * (extra="forbid") and a Result model, plus 13 WireEnum closed sets.
 *
 * What such a declaration decides, and what this file ports:
 *
 *   methods          method -> (params model, result model) index
 *   enums            the closed string sets, member order preserved
 *   params_spec      one descriptor per params field
 *                    (required / tag / ref / choices / has_default / default)
 *   validate_params  shaped errors for unknown / missing / mistyped keys
 *   normalize_params validated params with every declared default filled in
 *   validate_result  the same top-level checks on the result side
 *   profile_scoped   methods whose params accept the optional `profile` key
 *
 * SEAMS (declared, not silent):
 *   - pydantic lax coercion (str->int, "yes"->true, 5.0->5) is NOT emulated:
 *     a value is judged by its JSON-native type.
 *   - a nested model body is checked as an object only; its fields belong to
 *     that model's own gate.
 *   - mcp.catalog / mcp.servers.list / mcp.servers.status take Params from
 *     common.ProfileParams, so they are owned by the common.py gate and are
 *     not in this table.
 *   - enum identity, JSON serialisation and all I/O stay in the caller.
 */
#ifndef NOC_W32_TOOLSMCPPL_39BF88_H
#define NOC_W32_TOOLSMCPPL_39BF88_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"<method>": {"params": "<Model>", "result": "<Model>"}, ...} */
json_t *w32_toolsmcppl_39bf88_methods(void);

/* {"<EnumName>": ["<value>", ...], ...} */
json_t *w32_toolsmcppl_39bf88_enums(void);

/* {"<field>": {"required", "nullable", "tag", "ref", "choices",
 *              "has_default", "default"}, ...} for the method's params model,
 * or the string "UNKNOWN_METHOD". */
json_t *w32_toolsmcppl_39bf88_params_spec(const char *method);

/* {"ok": bool, "errors": [{"loc", "type"}, ...]} (errors sorted by loc,type;
 * "type" is the normalised code: extra / missing / enum / type_str /
 * type_int / type_float / type_bool / type_list / type_dict / type_object). */
json_t *w32_toolsmcppl_39bf88_validate_params(const char *method, const json_t *params);

/* {"ok": true, "params": {every field, defaults filled}} or the invalid
 * verdict above. */
json_t *w32_toolsmcppl_39bf88_normalize_params(const char *method, const json_t *params);

/* Same verdict shape, evaluated against the method's result model. */
json_t *w32_toolsmcppl_39bf88_validate_result(const char *method, const json_t *result);

/* Sorted method names whose params model declares `profile`. */
json_t *w32_toolsmcppl_39bf88_profile_scoped(void);

#ifdef __cplusplus
}
#endif
#endif
