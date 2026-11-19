"""twin for gateway/platforms/bluebubbles.py:send_animation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/bluebubbles.py"
    PY_FN = "send_animation"
    C_FN = "send_animation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
