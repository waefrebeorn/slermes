"""twin for gateway/profile_routing.py:parse_profile_routes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/profile_routing.py"
    PY_FN = "parse_profile_routes"
    C_FN = "gateway_profile_routing_parse_profile_routes"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
