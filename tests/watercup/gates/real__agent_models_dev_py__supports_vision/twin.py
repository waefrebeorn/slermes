"""twin for agent/models_dev.py:supports_vision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "supports_vision"
    C_FN = "model_supports_vision"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
