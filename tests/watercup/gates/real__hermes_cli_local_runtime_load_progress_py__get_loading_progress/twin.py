"""twin for hermes_cli/local_runtime/load_progress.py:get_loading_progress (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/load_progress.py"
    PY_FN = "get_loading_progress"
    C_FN = "get_loading_progress"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
