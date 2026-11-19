"""twin for tools/schema_sanitizer.py:collapse_const_unions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/schema_sanitizer.py"
    PY_FN = "collapse_const_unions"
    C_FN = "collapse_const_unions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
