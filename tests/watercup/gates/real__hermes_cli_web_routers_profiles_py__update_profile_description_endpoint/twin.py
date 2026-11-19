"""twin for hermes_cli/web_routers/profiles.py:update_profile_description_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/profiles.py"
    PY_FN = "update_profile_description_endpoint"
    C_FN = "update_profile_description_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
