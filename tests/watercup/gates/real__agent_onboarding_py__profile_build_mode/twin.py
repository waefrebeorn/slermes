"""twin for agent/onboarding.py:profile_build_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/onboarding.py"
    PY_FN = "profile_build_mode"
    C_FN = "profile_build_mode"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
