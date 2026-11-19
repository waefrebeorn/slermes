"""twin for hermes_cli/models.py:_refresh_nous_reasoning_caps (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_refresh_nous_reasoning_caps"
    C_FN = "models_refresh_nous_reasoning_caps"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
