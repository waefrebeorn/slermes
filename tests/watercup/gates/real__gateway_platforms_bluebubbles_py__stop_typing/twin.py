"""twin for gateway/platforms/bluebubbles.py:stop_typing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/bluebubbles.py"
    PY_FN = "stop_typing"
    C_FN = "stop_typing"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
