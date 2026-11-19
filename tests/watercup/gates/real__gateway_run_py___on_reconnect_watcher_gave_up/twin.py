"""twin for gateway/run.py:_on_reconnect_watcher_gave_up (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_on_reconnect_watcher_gave_up"
    C_FN = "grun_u_on_reconnect_watcher_gave_up"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
