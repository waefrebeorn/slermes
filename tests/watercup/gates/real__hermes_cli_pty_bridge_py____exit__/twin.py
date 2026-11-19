"""twin for hermes_cli/pty_bridge.py:__exit__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pty_bridge.py"
    PY_FN = "__exit__"
    C_FN = "PTY"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
