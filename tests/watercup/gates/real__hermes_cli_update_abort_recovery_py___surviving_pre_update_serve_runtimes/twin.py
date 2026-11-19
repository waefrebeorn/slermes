"""twin for hermes_cli/update_abort_recovery.py:_surviving_pre_update_serve_runtimes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_abort_recovery.py"
    PY_FN = "_surviving_pre_update_serve_runtimes"
    C_FN = "_surviving_pre_update_serve_runtimes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
