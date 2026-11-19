"""twin for gateway/platforms/helpers.py:balance_fences_across_chunks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/helpers.py"
    PY_FN = "balance_fences_across_chunks"
    C_FN = "balance_fences_across_chunks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
