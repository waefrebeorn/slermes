"""twin for hermes_cli/pty_bridge.py:spawn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pty_bridge.py"
    PY_FN = "spawn"
    C_FN = "spawn"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
