"""twin for gateway/run.py:_spawn_reconnect_watcher (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_spawn_reconnect_watcher"
    C_FN = "grun_u_spawn_reconnect_watcher"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
