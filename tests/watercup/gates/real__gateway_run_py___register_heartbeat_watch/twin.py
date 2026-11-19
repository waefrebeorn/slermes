"""twin for gateway/run.py:_register_heartbeat_watch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_register_heartbeat_watch"
    C_FN = "gw_register_heartbeat_watch"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
