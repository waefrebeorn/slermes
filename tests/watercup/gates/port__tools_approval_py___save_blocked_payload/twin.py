"""twin for tools/approval.py:_save_blocked_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_save_blocked_payload"
    C_FN = "_save_blocked_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
