"""twin for gateway/browser_control_artifacts.py:max_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/browser_control_artifacts.py"
    PY_FN = "max_bytes"
    C_FN = "max_bytes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
