"""twin for hermes_cli/models.py:get_preferred_silent_default_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "get_preferred_silent_default_model"
    C_FN = "get_preferred_silent_default_model"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
