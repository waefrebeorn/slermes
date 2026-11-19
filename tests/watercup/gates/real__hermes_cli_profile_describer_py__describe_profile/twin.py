"""twin for hermes_cli/profile_describer.py:describe_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_describer.py"
    PY_FN = "describe_profile"
    C_FN = "describe_profile"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
