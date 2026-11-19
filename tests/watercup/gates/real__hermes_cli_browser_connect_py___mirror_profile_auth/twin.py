"""twin for hermes_cli/browser_connect.py:_mirror_profile_auth (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/browser_connect.py"
    PY_FN = "_mirror_profile_auth"
    C_FN = "_mirror_profile_auth"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
