"""twin for hermes_cli/_early_recovery.py:restore_quarantined_shims (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_early_recovery.py"
    PY_FN = "restore_quarantined_shims"
    C_FN = "restore_quarantined_shims"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
