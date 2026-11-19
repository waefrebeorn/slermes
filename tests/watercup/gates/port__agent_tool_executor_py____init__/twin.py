"""twin for agent/tool_executor.py:__init__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_executor.py"
    PY_FN = "__init__"
    C_FN = "tex_concurrent_tool_authorization_gate_init"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
