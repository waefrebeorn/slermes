"""twin for agent/moa_loop.py:_truncate_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_truncate_tool_result"
    C_FN = "moa_u_truncate_tool_result"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
