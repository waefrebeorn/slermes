"""twin for hermes_cli/local_runtime/load_progress.py:_apply_event (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/load_progress.py"
    PY_FN = "_apply_event"
    C_FN = "_apply_event"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
