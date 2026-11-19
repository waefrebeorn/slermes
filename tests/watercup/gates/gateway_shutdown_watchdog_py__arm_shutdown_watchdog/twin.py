"""twin for gateway/shutdown_watchdog.py:arm_shutdown_watchdog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_watchdog.py"
    PY_FN = "arm_shutdown_watchdog"
    C_FN = "gateway_shutdown_watchdog_arm_shutdown_watchdog"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
