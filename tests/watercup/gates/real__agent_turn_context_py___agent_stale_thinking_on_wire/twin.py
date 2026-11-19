"""twin for agent/turn_context.py:_agent_stale_thinking_on_wire (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_context.py"
    PY_FN = "_agent_stale_thinking_on_wire"
    C_FN = "_agent_stale_thinking_on_wire"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
