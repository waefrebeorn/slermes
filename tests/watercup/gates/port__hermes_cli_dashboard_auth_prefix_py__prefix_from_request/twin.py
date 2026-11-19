"""twin for hermes_cli/dashboard_auth/prefix.py:prefix_from_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/prefix.py"
    PY_FN = "prefix_from_request"
    C_FN = "prefix_from_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
