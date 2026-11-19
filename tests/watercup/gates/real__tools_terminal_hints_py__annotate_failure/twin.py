"""twin for tools/terminal_hints.py:annotate_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_hints.py"
    PY_FN = "annotate_failure"
    C_FN = "annotate_failure"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
