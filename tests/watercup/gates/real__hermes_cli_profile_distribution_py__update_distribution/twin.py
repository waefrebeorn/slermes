"""twin for hermes_cli/profile_distribution.py:update_distribution (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_distribution.py"
    PY_FN = "update_distribution"
    C_FN = "update_distribution"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
