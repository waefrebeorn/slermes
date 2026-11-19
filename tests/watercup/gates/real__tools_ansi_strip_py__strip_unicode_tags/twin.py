"""twin for tools/ansi_strip.py:strip_unicode_tags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/ansi_strip.py"
    PY_FN = "strip_unicode_tags"
    C_FN = "strip_unicode_tags"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
