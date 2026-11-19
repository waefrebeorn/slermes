"""twin for agent/models_dev.py:_background_refresh_models_dev (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_background_refresh_models_dev"
    C_FN = "mdv_background_refresh_models_dev"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
