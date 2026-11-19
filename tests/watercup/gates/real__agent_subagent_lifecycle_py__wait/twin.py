"""twin for agent/subagent_lifecycle.py:wait (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subagent_lifecycle.py"
    PY_FN = "wait"
    C_FN = "wait"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
