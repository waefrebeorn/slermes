"""twin for hermes_cli/local_runtime/bootstrap.py:_start_idle_sweeper (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/bootstrap.py"
    PY_FN = "_start_idle_sweeper"
    C_FN = "_start_idle_sweeper"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
