"""twin for gateway/platforms/qqbot/adapter.py:_create_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/adapter.py"
    PY_FN = "_create_task"
    C_FN = "_create_task"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
