"""twin for hermes_cli/timeouts.py:_get_model_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/timeouts.py"
    PY_FN = "_get_model_config"
    C_FN = "_get_model_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
