"""twin for gateway/run.py:_deliver_async_delegation_group (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_deliver_async_delegation_group"
    C_FN = "_deliver_async_delegation_group"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
