"""twin for hermes_cli/debug.py:_redact_log_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/debug.py"
    PY_FN = "_redact_log_text"
    C_FN = "debug_redact_log_text"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
