"""twin for hermes_cli/local_runtime/hf_browse.py:search_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/hf_browse.py"
    PY_FN = "search_models"
    C_FN = "search_models"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
