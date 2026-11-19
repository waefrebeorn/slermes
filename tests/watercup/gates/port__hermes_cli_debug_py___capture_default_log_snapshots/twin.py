"""twin for hermes_cli/debug.py:_capture_default_log_snapshots (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/debug.py"
    PY_FN = "_capture_default_log_snapshots"
    C_FN = "hermes_cli_debug_u_capture_default_log_snapshots"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
