"""twin for hermes_cli/web_server.py:_dashboard_local_update_managed_externally (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_dashboard_local_update_managed_externally"
    C_FN = "dashboard_local_update_managed_externally"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
