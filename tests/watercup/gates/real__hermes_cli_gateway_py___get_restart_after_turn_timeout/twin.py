"""twin for hermes_cli/gateway.py:_get_restart_after_turn_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_get_restart_after_turn_timeout"
    C_FN = "cgw_get_restart_after_turn_timeout"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
