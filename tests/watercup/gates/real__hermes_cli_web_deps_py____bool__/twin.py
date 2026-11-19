"""twin for hermes_cli/web_deps.py:__bool__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_deps.py"
    PY_FN = "__bool__"
    C_FN = "__bool__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
