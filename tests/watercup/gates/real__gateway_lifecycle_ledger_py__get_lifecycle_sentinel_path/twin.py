"""twin for gateway/lifecycle_ledger.py:get_lifecycle_sentinel_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/lifecycle_ledger.py"
    PY_FN = "get_lifecycle_sentinel_path"
    C_FN = "get_lifecycle_sentinel_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
