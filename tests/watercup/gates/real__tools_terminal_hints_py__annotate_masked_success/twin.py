"""twin for tools/terminal_hints.py:annotate_masked_success (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_hints.py"
    PY_FN = "annotate_masked_success"
    C_FN = "annotate_masked_success"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
