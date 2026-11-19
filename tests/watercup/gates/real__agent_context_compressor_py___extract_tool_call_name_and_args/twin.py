"""twin for agent/context_compressor.py:_extract_tool_call_name_and_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_extract_tool_call_name_and_args"
    C_FN = "_extract_tool_call_name_and_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
