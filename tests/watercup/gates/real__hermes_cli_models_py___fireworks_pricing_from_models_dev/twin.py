"""twin for hermes_cli/models.py:_fireworks_pricing_from_models_dev (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_fireworks_pricing_from_models_dev"
    C_FN = "_fireworks_pricing_from_models_dev"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
