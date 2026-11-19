"""twin for agent/models_dev.py:_get_provider_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_get_provider_models"
    C_FN = "_get_provider_models"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
