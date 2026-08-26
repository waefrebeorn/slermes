/* Link shims: off-path symbols referenced by cua_backend_methods.c that this
 * gate's traced function (_windows_from_tool_result) never executes. */
#include <stddef.h>
char *cua_backend_resolve_cua_driver_cmd(void) { return NULL; }
char *cua_backend_cua_driver_runtime_contract_status(void) { return NULL; }
