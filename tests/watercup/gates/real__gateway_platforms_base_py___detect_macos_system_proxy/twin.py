"""twin for gateway/platforms/base.py:_detect_macos_system_proxy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_detect_macos_system_proxy"
    C_FN = "_detect_macos_system_proxy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
