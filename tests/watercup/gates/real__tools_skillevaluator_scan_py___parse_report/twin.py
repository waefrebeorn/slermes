"""twin for tools/skillevaluator_scan.py:_parse_report (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skillevaluator_scan.py"
    PY_FN = "_parse_report"
    C_FN = "_parse_report"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
