"""twin for hermes_cli/win_pty_bridge.py:is_alive (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/win_pty_bridge.py"
    PY_FN = "is_alive"
    C_FN = "is_alive"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
