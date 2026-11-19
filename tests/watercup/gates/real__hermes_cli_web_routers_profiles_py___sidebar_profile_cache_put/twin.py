"""twin for hermes_cli/web_routers/profiles.py:_sidebar_profile_cache_put (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/profiles.py"
    PY_FN = "_sidebar_profile_cache_put"
    C_FN = "_sidebar_profile_cache_put"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
