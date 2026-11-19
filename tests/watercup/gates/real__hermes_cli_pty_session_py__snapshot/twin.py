"""twin for hermes_cli/pty_session.py:snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pty_session.py"
    PY_FN = "snapshot"
    C_FN = "pty_snapshot"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
