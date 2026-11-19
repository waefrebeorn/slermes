"""twin for hermes_cli/main.py:_quarantine_stamp_ms (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_quarantine_stamp_ms"
    C_FN = "mn_quarantine_stamp_ms"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
