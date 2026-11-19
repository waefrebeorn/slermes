"""twin for gateway/pairing.py:approve_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "approve_request"
    C_FN = "approve_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
