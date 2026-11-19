"""twin for gateway/display_config.py:_normalise (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/display_config.py"
    PY_FN = "_normalise"
    C_FN = "_normalise"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
