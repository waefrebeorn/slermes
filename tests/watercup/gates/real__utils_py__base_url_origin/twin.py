"""twin for utils.py:base_url_origin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "utils.py"
    PY_FN = "base_url_origin"
    C_FN = "base_url_origin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
