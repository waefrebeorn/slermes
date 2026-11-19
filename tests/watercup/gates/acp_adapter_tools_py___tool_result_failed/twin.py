"""twin for acp_adapter/tools.py:_tool_result_failed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "acp_adapter/tools.py"
    PY_FN = "_tool_result_failed"
    C_FN = "acpt_tool_result_failed"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
