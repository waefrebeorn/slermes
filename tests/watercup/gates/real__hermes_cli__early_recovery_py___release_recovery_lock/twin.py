"""twin for hermes_cli/_early_recovery.py:_release_recovery_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_early_recovery.py"
    PY_FN = "_release_recovery_lock"
    C_FN = "_release_recovery_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
