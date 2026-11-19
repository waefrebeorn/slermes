"""twin for gateway/profile_routing.py:_coerce_route_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/profile_routing.py"
    PY_FN = "_coerce_route_id"
    C_FN = "_coerce_route_id"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
