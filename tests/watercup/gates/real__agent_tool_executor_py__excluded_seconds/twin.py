"""twin for agent/tool_executor.py:excluded_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "excluded_seconds"
    C_FN = "tex_concurrent_tool_authorization_gate_excluded_seconds"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
