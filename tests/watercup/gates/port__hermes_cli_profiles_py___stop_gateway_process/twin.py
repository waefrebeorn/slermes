"""twin for hermes_cli/profiles.py:_stop_gateway_process (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_stop_gateway_process"
    C_FN = "profile_stop_gateway_process"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
