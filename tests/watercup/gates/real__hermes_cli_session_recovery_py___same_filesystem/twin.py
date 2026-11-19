"""twin for hermes_cli/session_recovery.py:_same_filesystem (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_same_filesystem"
    C_FN = "sr_same_filesystem"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
