"""twin for tools/osv_check.py:_infer_ecosystem (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/osv_check.py"
    PY_FN = "_infer_ecosystem"
    C_FN = "_infer_ecosystem"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
