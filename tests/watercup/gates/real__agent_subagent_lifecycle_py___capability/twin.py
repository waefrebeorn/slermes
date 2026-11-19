"""twin for agent/subagent_lifecycle.py:_capability (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subagent_lifecycle.py"
    PY_FN = "_capability"
    C_FN = "_capability"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
