"""twin for tools/skillevaluator_scan.py:secrets_findings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skillevaluator_scan.py"
    PY_FN = "secrets_findings"
    C_FN = "secrets_findings"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
