"""twin for gateway/run.py:_snapshot_profile_busy_modes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_snapshot_profile_busy_modes"
    C_FN = "gw_snapshot_profile_busy_modes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
