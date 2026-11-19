"""twin for gateway/lifecycle_ledger.py:_read_json (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/lifecycle_ledger.py"
    PY_FN = "_read_json"
    C_FN = "_read_json"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
