"""twin for gateway/platforms/base.py:safe_url_for_log (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "safe_url_for_log"
    C_FN = "safe_url_for_log"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
