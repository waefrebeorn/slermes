"""twin for hermes_cli/local_runtime/growth.py:load_window_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/growth.py"
    PY_FN = "load_window_overrides"
    C_FN = "load_window_overrides"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
