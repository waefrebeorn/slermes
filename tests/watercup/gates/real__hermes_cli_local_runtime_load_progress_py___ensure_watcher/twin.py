"""twin for hermes_cli/local_runtime/load_progress.py:_ensure_watcher (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/load_progress.py"
    PY_FN = "_ensure_watcher"
    C_FN = "_ensure_watcher"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
