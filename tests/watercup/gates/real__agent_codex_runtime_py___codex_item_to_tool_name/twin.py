"""twin for agent/codex_runtime.py:_codex_item_to_tool_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_runtime.py"
    PY_FN = "_codex_item_to_tool_name"
    C_FN = "agent_codex_runtime_u_codex_item_to_tool_name"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
