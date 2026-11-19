"""twin for hermes_cli/_startup_fast.py:_resolved_home (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"
    PY_FN = "_resolved_home"
    C_FN = "_resolved_home"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
