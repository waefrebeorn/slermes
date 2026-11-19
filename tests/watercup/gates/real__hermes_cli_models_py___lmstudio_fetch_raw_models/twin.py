"""twin for hermes_cli/models.py:_lmstudio_fetch_raw_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_lmstudio_fetch_raw_models"
    C_FN = "_lmstudio_fetch_raw_models"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
