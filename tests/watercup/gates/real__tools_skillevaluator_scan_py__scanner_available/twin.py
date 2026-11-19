"""twin for tools/skillevaluator_scan.py:scanner_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skillevaluator_scan.py"
    PY_FN = "scanner_available"
    C_FN = "scanner_available"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
