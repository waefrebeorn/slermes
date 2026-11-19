"""twin for hermes_cli/skin_engine.py:get_active_skin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skin_engine.py"
    PY_FN = "get_active_skin"
    C_FN = "get_active_skin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
