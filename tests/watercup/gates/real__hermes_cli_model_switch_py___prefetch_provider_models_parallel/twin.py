"""twin for hermes_cli/model_switch.py:_prefetch_provider_models_parallel (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "_prefetch_provider_models_parallel"
    C_FN = "_prefetch_provider_models_parallel"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
