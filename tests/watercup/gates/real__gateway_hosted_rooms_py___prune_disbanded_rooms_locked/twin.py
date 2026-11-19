"""twin for gateway/hosted_rooms.py:_prune_disbanded_rooms_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "_prune_disbanded_rooms_locked"
    C_FN = "_prune_disbanded_rooms_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
