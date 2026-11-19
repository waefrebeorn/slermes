"""twin for hermes_cli/models.py:clear_provider_models_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "clear_provider_models_cache"
    C_FN = "model_clear_provider_models_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
