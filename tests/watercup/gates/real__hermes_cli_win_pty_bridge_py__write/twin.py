"""twin for hermes_cli/win_pty_bridge.py:write (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/win_pty_bridge.py"
    PY_FN = "write"
    C_FN = "write"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
