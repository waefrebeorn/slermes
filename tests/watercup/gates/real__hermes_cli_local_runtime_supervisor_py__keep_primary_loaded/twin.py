"""twin for hermes_cli/local_runtime/supervisor.py:keep_primary_loaded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/supervisor.py"
    PY_FN = "keep_primary_loaded"
    C_FN = "keep_primary_loaded"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
