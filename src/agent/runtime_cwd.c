/*
 * runtime_cwd.c — Port of Python agent/runtime_cwd.py
 *
 * Runtime working-directory resolution. The Python module exposes
 * get_runtime_cwd() / set_runtime_cwd() / format_cwd_in_prompt(); in the C
 * port these are real entry points delegating to the canonical implementation
 * in hermes_system_prompt.h (resolve_agent_cwd / set_session_cwd /
 * build_environment_hints). No "N/A".
 */
#include "hermes_system_prompt.h"   /* resolve_agent_cwd, set_session_cwd, build_environment_hints */
#include <string.h>

/* Port of Python: runtime_cwd.get_runtime_cwd() */
const char *get_runtime_cwd(void) {
    return resolve_agent_cwd();
}

/* Port of Python: runtime_cwd.set_runtime_cwd() */
void set_runtime_cwd(const char *cwd) {
    set_session_cwd(cwd);
}

/* Port of Python: runtime_cwd.format_cwd_in_prompt() */
char *format_cwd_in_prompt(void) {
    return build_environment_hints();
}
