"""twin for gateway/profile_routing.py:matches (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/profile_routing.py"
    PY_FN = "matches"
    C_FN = "gw_profile_route_matches"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
