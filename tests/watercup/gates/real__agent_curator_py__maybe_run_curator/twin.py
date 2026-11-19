"""twin for agent/curator.py:maybe_run_curator (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator.py"
    PY_FN = "maybe_run_curator"
    C_FN = "maybe_run_curator"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
