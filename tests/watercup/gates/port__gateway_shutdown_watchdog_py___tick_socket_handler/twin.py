"""twin for gateway/shutdown_watchdog.py:_tick_socket_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_watchdog.py"
    PY_FN = "_tick_socket_handler"
    C_FN = "_tick_socket_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
