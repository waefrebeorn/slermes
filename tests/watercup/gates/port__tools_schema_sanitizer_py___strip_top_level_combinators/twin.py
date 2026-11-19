"""twin for tools/schema_sanitizer.py:_strip_top_level_combinators (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/schema_sanitizer.py"
    PY_FN = "_strip_top_level_combinators"
    C_FN = "_strip_top_level_combinators"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
