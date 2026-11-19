"""twin for agent/tool_dispatch_helpers.py:_neutralize_delimiters (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"
    PY_FN = "_neutralize_delimiters"
    C_FN = "_neutralize_delimiters"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
