"""twin for tools/xai_http.py:has_xai_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "has_xai_credentials"
    C_FN = "has_xai_credentials"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
