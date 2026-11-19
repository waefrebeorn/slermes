"""twin for gateway/lifecycle_ledger.py:detect_unclean_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/lifecycle_ledger.py"
    PY_FN = "detect_unclean_exit"
    C_FN = "detect_unclean_exit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
