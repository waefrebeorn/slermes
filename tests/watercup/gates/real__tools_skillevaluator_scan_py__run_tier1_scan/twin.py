"""twin for tools/skillevaluator_scan.py:run_tier1_scan (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skillevaluator_scan.py"
    PY_FN = "run_tier1_scan"
    C_FN = "run_tier1_scan"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
