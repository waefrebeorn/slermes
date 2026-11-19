"""twin for hermes_cli/browser_connect.py:close_browser_holding_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/browser_connect.py"
    PY_FN = "close_browser_holding_profile"
    C_FN = "close_browser_holding_profile"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
