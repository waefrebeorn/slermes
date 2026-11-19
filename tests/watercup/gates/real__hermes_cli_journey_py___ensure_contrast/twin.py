"""twin for hermes_cli/journey.py:_ensure_contrast (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/journey.py"
    PY_FN = "_ensure_contrast"
    C_FN = "_ensure_contrast"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
