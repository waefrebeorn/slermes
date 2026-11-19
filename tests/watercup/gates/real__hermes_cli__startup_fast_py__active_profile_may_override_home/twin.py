"""twin for hermes_cli/_startup_fast.py:active_profile_may_override_home (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"
    PY_FN = "active_profile_may_override_home"
    C_FN = "sf_active_profile_may_override_home"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
