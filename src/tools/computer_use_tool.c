/*
 * computer_use_tool.c — C11 port of tools/computer_use_tool.py
 *
 * Shim for tool discovery. Registers `computer_use` with tools.registry.
 * The real implementation lives in src/tools/computer_use.c (the port of the
 * tools/computer_use/ package); this shim exists because the registry
 * auto-imports top-level tools/*.py modules — we need one to trigger the
 * registration, exactly as Python does.
 */

#define _POSIX_C_SOURCE 200809L

#include "computer_use_tool.h"
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real implementation (src/tools/computer_use.c). */
extern char *handle_computer_use(const char *args_json, const char *task_id);
extern int cu_has_global_backend(void);
void cu_reset_global_backend(void);
/* src/hermes_cli/tools_config_cua.c — port of cua_backend.resolve_cua_driver_cmd */
char *cua_backend_resolve_cua_driver_cmd(const char *override);
/* src/tools/computer_use.c — port of schema.get_computer_use_schema */
char *computer_use_get_computer_use_schema(void);

/* Handler lambda: `lambda args, **kw: handle_computer_use(args, **kw)`. */
static char *computer_use_tool_handler(const char *args_json,
                                       const char *task_id) {
    return handle_computer_use(args_json, task_id);
}

bool computer_use_tool_register(void) {
    char *schema = computer_use_get_computer_use_schema();
    if (!schema) return false;
    /* Description is the shim's own registry.register description string,
     * verbatim from tools/computer_use_tool.py. requires_env=[]. */
    bool ok = registry_register_ex_full(
        "computer_use",
        "Universal desktop control via cua-driver (macOS, Windows, Linux). Works with any "
        "tool-capable model (Anthropic, OpenAI, OpenRouter, local vLLM, "
        "etc.). Background computer-use: does NOT steal the user's cursor "
        "or keyboard focus.",
        schema,
        "computer_use",
        computer_use_tool_handler,
        NULL,
        0,
        REGISTRY_DEFAULT_RESULT_SIZE_CHARS);
    free(schema);
    return ok;
}

char *computer_use_tool_handle_computer_use(const char *args_json,
                                            const char *task_id) {
    return handle_computer_use(args_json, task_id);
}

int computer_use_tool_check_computer_use_requirements(void) {
    /* sys.platform check: this C11 target is linux — always passes.
     * Remaining condition: cua-driver binary available (env override, PATH,
     * or known install paths). */
    char *cmd = cua_backend_resolve_cua_driver_cmd(NULL);
    if (!cmd) return 0;
    free(cmd);
    return 1;
}

int computer_use_tool_release_computer_use_session(const char *session_id) {
    (void)session_id;  /* C backend layer caches a single global backend */
    int had = cu_has_global_backend();
    cu_reset_global_backend();
    return had;
}
