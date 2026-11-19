"""twin for agent/replay_cleanup.py:strip_stale_dangerous_confirmations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/replay_cleanup.py"
    PY_FN = "strip_stale_dangerous_confirmations"
    C_FN = "strip_stale_dangerous_confirmations"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
