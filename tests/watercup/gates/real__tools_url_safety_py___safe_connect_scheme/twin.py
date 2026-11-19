"""twin for tools/url_safety.py:_safe_connect_scheme (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "_safe_connect_scheme"
    C_FN = "_safe_connect_scheme"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
