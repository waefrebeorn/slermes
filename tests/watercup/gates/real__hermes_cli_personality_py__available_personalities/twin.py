"""twin for hermes_cli/personality.py:available_personalities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/personality.py"
    PY_FN = "available_personalities"
    C_FN = "available_personalities"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
