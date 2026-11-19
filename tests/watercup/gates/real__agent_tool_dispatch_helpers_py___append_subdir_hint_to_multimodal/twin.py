"""twin for agent/tool_dispatch_helpers.py:_append_subdir_hint_to_multimodal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"
    PY_FN = "_append_subdir_hint_to_multimodal"
    C_FN = "agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
