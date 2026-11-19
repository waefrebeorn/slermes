"""twin for hermes_cli/status.py:_configured_model_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/status.py"
    PY_FN = "_configured_model_label"
    C_FN = "sta_configured_model_label"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
