"""twin for hermes_cli/session_recovery.py:_cleanup_partial_orphans (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_cleanup_partial_orphans"
    C_FN = "_cleanup_partial_orphans"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
