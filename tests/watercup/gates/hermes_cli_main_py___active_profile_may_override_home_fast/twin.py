"""twin for hermes_cli/main.py:_active_profile_may_override_home_fast (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_active_profile_may_override_home_fast"
    C_FN = "main_g_active_profile_may_override_home_fast"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
