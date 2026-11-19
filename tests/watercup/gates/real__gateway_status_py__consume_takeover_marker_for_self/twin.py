"""twin for gateway/status.py:consume_takeover_marker_for_self (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "consume_takeover_marker_for_self"
    C_FN = "gwstatus_consume_takeover_marker_for_self"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
