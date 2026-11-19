"""twin for hermes_cli/active_sessions.py:_process_start_time (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/active_sessions.py"
    PY_FN = "_process_start_time"
    C_FN = "hermes_cli_active_sessions_u_process_start_time"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
