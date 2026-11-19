"""twin for hermes_cli/lifecycle.py:finalize_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/lifecycle.py"
    PY_FN = "finalize_session"
    C_FN = "hermes_cli_lifecycle_finalize_session"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
