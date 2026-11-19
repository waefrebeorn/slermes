"""twin for hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/profiles.py"
    PY_FN = "describe_profile_auto_endpoint"
    C_FN = "describe_profile_auto_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
