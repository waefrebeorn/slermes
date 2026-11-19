"""twin for agent/auxiliary_client.py:_warn_paid_lane_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_warn_paid_lane_once"
    C_FN = "auxc_warn_paid_lane_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
