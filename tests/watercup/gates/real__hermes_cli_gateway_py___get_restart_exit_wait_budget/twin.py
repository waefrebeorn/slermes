"""twin for hermes_cli/gateway.py:_get_restart_exit_wait_budget (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_get_restart_exit_wait_budget"
    C_FN = "cgw_get_restart_exit_wait_budget"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
