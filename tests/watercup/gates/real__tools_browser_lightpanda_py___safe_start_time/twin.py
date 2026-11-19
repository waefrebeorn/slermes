"""twin for tools/browser_lightpanda.py:_safe_start_time (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_lightpanda.py"
    PY_FN = "_safe_start_time"
    C_FN = "_safe_start_time"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
