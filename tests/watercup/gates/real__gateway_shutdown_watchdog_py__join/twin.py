"""twin for gateway/shutdown_watchdog.py:join (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_watchdog.py"
    PY_FN = "join"
    C_FN = "join"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
