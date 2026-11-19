"""twin for hermes_cli/web_routers/profiles.py:_warn_profile_read_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/profiles.py"
    PY_FN = "_warn_profile_read_error"
    C_FN = "profiles_warn_read_error"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
