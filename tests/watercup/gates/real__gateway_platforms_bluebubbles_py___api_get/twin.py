"""twin for gateway/platforms/bluebubbles.py:_api_get (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/bluebubbles.py"
    PY_FN = "_api_get"
    C_FN = "_api_get"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
