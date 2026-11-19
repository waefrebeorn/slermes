"""twin for agent/context_compressor.py:_summarize_tool_result_unguarded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_summarize_tool_result_unguarded"
    C_FN = "_summarize_tool_result_unguarded"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
