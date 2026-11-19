"""twin for cron/lifecycle_guard.py:_resolve_terminal_script_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_resolve_terminal_script_path"
    C_FN = "_resolve_terminal_script_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
