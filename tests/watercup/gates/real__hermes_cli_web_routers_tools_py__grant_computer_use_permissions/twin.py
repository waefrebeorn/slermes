"""twin for hermes_cli/web_routers/tools.py:grant_computer_use_permissions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/tools.py"
    PY_FN = "grant_computer_use_permissions"
    C_FN = "grant_computer_use_permissions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
