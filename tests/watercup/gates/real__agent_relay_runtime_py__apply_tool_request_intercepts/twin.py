"""twin for agent/relay_runtime.py:apply_tool_request_intercepts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "apply_tool_request_intercepts"
    C_FN = "apply_tool_request_intercepts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
