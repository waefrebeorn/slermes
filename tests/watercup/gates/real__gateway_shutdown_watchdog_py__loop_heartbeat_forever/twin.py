"""twin for gateway/shutdown_watchdog.py:loop_heartbeat_forever (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_watchdog.py"
    PY_FN = "loop_heartbeat_forever"
    C_FN = "gateway_shutdown_watchdog_loop_heartbeat_forever"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
