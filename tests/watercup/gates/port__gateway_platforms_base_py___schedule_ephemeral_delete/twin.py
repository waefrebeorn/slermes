"""twin for gateway/platforms/base.py:_schedule_ephemeral_delete (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_schedule_ephemeral_delete"
    C_FN = "_schedule_ephemeral_delete"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
