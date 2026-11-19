"""twin for hermes_cli/local_runtime/supervisor.py:sweep_idle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/supervisor.py"
    PY_FN = "sweep_idle"
    C_FN = "sweep_idle"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
