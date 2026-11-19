"""twin for hermes_cli/completion.py:_clean (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/completion.py"
    PY_FN = "_clean"
    C_FN = "_clean"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
