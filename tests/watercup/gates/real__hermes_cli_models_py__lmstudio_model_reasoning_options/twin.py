"""twin for hermes_cli/models.py:lmstudio_model_reasoning_options (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "lmstudio_model_reasoning_options"
    C_FN = "lmstudio_model_reasoning_options"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
