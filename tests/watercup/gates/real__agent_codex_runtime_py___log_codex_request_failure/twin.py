"""twin for agent/codex_runtime.py:_log_codex_request_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/codex_runtime.py"
    PY_FN = "_log_codex_request_failure"
    C_FN = "_log_codex_request_failure"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
