"""twin for hermes_cli/terminal_notify.py:notify (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/terminal_notify.py"
    PY_FN = "notify"
    C_FN = "notify"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
