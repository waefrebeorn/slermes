"""twin for tools/mcp_death_supervisor.py:_serve (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_death_supervisor.py"
    PY_FN = "_serve"
    C_FN = "_serve"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
