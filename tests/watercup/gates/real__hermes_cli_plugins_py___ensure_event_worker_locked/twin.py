"""twin for hermes_cli/plugins.py:_ensure_event_worker_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_ensure_event_worker_locked"
    C_FN = "_ensure_event_worker_locked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
