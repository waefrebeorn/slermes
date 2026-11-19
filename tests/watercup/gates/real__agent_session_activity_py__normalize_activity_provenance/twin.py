"""twin for agent/session_activity.py:normalize_activity_provenance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/session_activity.py"
    PY_FN = "normalize_activity_provenance"
    C_FN = "normalize_activity_provenance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
