"""twin for agent/process_bootstrap.py:flush (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"
    PY_FN = "flush"
    C_FN = "safe_writer_flush"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
