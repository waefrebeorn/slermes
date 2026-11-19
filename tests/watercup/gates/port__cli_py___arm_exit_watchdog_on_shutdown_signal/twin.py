"""twin for cli.py:_arm_exit_watchdog_on_shutdown_signal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_arm_exit_watchdog_on_shutdown_signal"
    C_FN = "_arm_exit_watchdog_on_shutdown_signal"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
