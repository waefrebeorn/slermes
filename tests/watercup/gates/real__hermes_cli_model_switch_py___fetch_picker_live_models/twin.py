"""twin for hermes_cli/model_switch.py:_fetch_picker_live_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "_fetch_picker_live_models"
    C_FN = "_fetch_picker_live_models"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
