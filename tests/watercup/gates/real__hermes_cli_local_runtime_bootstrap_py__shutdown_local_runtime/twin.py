"""twin for hermes_cli/local_runtime/bootstrap.py:shutdown_local_runtime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/bootstrap.py"
    PY_FN = "shutdown_local_runtime"
    C_FN = "shutdown_local_runtime"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
