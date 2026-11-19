"""twin for gateway/platforms/signal.py:_remux_aac_to_m4a (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_remux_aac_to_m4a"
    C_FN = "_remux_aac_to_m4a"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
