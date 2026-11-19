"""twin for gateway/platforms/bluebubbles.py:_send_attachment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/bluebubbles.py"
    PY_FN = "_send_attachment"
    C_FN = "_send_attachment"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
