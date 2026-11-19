"""twin for agent/relay_runtime.py:run_in_session_async (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "run_in_session_async"
    C_FN = "relay_run_in_session_async"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
