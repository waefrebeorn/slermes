"""twin for agent/context_compressor.py:_truncate_tool_call_args_json (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_truncate_tool_call_args_json"
    C_FN = "_truncate_tool_call_args_json"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
