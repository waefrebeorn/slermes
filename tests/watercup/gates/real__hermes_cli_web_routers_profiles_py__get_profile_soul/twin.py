"""twin for hermes_cli/web_routers/profiles.py:get_profile_soul (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/profiles.py"
    PY_FN = "get_profile_soul"
    C_FN = "get_profile_soul"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
