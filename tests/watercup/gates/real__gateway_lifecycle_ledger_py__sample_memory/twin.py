"""twin for gateway/lifecycle_ledger.py:sample_memory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/lifecycle_ledger.py"
    PY_FN = "sample_memory"
    C_FN = "sample_memory"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
