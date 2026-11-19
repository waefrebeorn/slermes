"""twin for hermes_cli/web_deps.py:__iter__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_deps.py"
    PY_FN = "__iter__"
    C_FN = "__iter__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
