"""twin for utils.py:model_forces_max_completion_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "utils.py"
    PY_FN = "model_forces_max_completion_tokens"
    C_FN = "util_model_forces_max_completion_tokens"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
