"""twin for tools/plugin_guard.py:_filter_findings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/plugin_guard.py"
    PY_FN = "_filter_findings"
    C_FN = "_filter_findings"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
