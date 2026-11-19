"""twin for tools/delegation_live_log.py:tool_start (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegation_live_log.py"
    PY_FN = "tool_start"
    C_FN = "tools_delegation_live_log_tool_start"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
