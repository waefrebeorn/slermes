"""twin for hermes_cli/profile_distribution.py:plan_install (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_distribution.py"
    PY_FN = "plan_install"
    C_FN = "plan_install"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
