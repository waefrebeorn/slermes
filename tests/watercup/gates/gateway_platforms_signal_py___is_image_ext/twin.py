"""twin for gateway/platforms/signal.py:_is_image_ext (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_is_image_ext"
    C_FN = "sig_is_image_ext"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
