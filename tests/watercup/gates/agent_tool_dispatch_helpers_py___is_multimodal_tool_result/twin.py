"""twin for agent/tool_dispatch_helpers.py:_is_multimodal_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"
    PY_FN = "_is_multimodal_tool_result"
    C_FN = "agent_tool_dispatch_helpers_u_is_multimodal_tool_result"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
