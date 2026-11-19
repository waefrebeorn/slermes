"""twin for hermes_cli/skin_engine.py:load_skin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skin_engine.py"
    PY_FN = "load_skin"
    C_FN = "load_skin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
