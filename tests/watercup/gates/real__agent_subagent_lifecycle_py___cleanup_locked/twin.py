"""twin for agent/subagent_lifecycle.py:_cleanup_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subagent_lifecycle.py"
    PY_FN = "_cleanup_locked"
    C_FN = "_cleanup_locked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
