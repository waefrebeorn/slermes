/* Dedicated port for tools/memory_tool.py:check_memory_requirements.
 * Split out of memory_tool.c so this dependency-free decision can be linked
 * and oracle-verified without dragging the whole tools registry into the gate.
 *
 * Python: _memory_surface_flags.set(None);
 * flags = get_builtin_memory_store_flags();   # (memory_enabled, user_profile_enabled)
 * _memory_surface_flags.set(flags);
 * return flags[0] or flags[1];
 * The contextvar snapshot is a Python-side side effect; the decision is this OR.
 */
#include "hermes_json.h"

/* PoP: memory_tool_available @ tools/memory_tool.py:check_memory_requirements */
int memory_tool_available(int memory_enabled, int user_profile_enabled) {
    return memory_enabled || user_profile_enabled;
}
