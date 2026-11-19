"""twin for agent/models_dev.py:_override_context_window (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_override_context_window"
    C_FN = "mdv_override_context_window"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
