"""twin for agent/deadline.py:resolve_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/deadline.py"
    PY_FN = "resolve_timeout"
    C_FN = "resolve_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
