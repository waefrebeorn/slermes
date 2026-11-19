"""twin for hermes_cli/nous_account.py:get_nous_portal_account_info (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_account.py"
    PY_FN = "get_nous_portal_account_info"
    C_FN = "get_nous_portal_account_info"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
