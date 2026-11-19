"""twin for gateway/platforms/webhook_filters.py:route_filters_match (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook_filters.py"
    PY_FN = "route_filters_match"
    C_FN = "gateway_platforms_webhook_filt_route_filters_match"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
