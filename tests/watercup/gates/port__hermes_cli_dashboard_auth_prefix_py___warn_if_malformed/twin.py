"""twin for hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/prefix.py"
    PY_FN = "_warn_if_malformed"
    C_FN = "cli_hermes_cli_dashboard_auth_prefix__warn_if_malformed"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
