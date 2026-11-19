"""twin for hermes_cli/pty_session.py:run_reaper (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pty_session.py"
    PY_FN = "run_reaper"
    C_FN = "hermes_cli_pty_session_run_reaper"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
