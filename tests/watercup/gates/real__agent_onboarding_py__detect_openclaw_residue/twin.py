"""twin for agent/onboarding.py:detect_openclaw_residue (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/onboarding.py"
    PY_FN = "detect_openclaw_residue"
    C_FN = "obd_detect_openclaw_residue"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
