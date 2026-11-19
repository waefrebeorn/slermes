"""twin for gateway/run.py:_watch_gateway_turn_inactivity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_watch_gateway_turn_inactivity"
    C_FN = "gw_watch_gateway_turn_inactivity"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
