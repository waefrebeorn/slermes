"""twin for tools/mcp_death_supervisor.py:_is_safe_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_death_supervisor.py"
    PY_FN = "_is_safe_target"
    C_FN = "_is_safe_target"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
