"""twin for tools/mcp_death_supervisor.py:_reap (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_death_supervisor.py"
    PY_FN = "_reap"
    C_FN = "_reap"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
