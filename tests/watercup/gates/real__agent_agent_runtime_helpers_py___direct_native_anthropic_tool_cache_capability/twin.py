"""twin for agent/agent_runtime_helpers.py:_direct_native_anthropic_tool_cache_capability (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_direct_native_anthropic_tool_cache_capability"
    C_FN = "_direct_native_anthropic_tool_cache_capability"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
