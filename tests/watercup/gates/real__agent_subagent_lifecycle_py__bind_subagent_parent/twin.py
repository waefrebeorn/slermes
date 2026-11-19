"""twin for agent/subagent_lifecycle.py:bind_subagent_parent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subagent_lifecycle.py"
    PY_FN = "bind_subagent_parent"
    C_FN = "subagent_bind_parent"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
