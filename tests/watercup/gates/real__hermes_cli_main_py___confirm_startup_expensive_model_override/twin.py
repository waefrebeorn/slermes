"""twin for hermes_cli/main.py:_confirm_startup_expensive_model_override (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_confirm_startup_expensive_model_override"
    C_FN = "_confirm_startup_expensive_model_override"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
