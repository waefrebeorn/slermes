"""twin for gateway/run.py:_approval_send_outcome (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_approval_send_outcome"
    C_FN = "_approval_send_outcome"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
