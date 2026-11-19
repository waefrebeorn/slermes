"""twin for hermes_cli/local_runtime/estimator.py:physics_check (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/estimator.py"
    PY_FN = "physics_check"
    C_FN = "physics_check"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
