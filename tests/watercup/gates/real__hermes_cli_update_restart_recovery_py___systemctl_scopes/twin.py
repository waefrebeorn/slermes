"""twin for hermes_cli/update_restart_recovery.py:_systemctl_scopes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_restart_recovery.py"
    PY_FN = "_systemctl_scopes"
    C_FN = "_systemctl_scopes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
