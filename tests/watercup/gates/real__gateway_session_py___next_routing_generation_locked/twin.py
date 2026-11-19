"""twin for gateway/session.py:_next_routing_generation_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_next_routing_generation_locked"
    C_FN = "_next_routing_generation_locked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
