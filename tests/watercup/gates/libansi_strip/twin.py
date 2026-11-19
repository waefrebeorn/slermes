"""twin for tools/ansi_strip.py:strip_ansi (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/ansi_strip.py"
    PY_FN = "strip_ansi"
    C_FN = "strip_ansi"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
