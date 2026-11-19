"""twin for agent/transports/codex_app_server_session.py:_format_error_with_stderr (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server_session.py"
    PY_FN = "_format_error_with_stderr"
    C_FN = "_format_error_with_stderr"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
