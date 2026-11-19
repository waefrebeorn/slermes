"""twin for tools/mcp_death_supervisor.py:main (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_death_supervisor.py"
    PY_FN = "main"
    C_FN = "main"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
