"""twin for agent/models_dev.py:format_capabilities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "format_capabilities"
    C_FN = "format_capabilities"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
