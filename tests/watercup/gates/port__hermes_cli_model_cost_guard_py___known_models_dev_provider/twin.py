"""twin for hermes_cli/model_cost_guard.py:_known_models_dev_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_cost_guard.py"
    PY_FN = "_known_models_dev_provider"
    C_FN = "model_cost_guard_known_models_dev_provider"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
