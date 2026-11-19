"""twin for agent/onboarding.py:is_seen (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/onboarding.py"
    PY_FN = "is_seen"
    C_FN = "obd_is_seen"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
