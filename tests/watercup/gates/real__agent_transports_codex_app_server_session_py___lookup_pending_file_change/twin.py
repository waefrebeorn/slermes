"""twin for agent/transports/codex_app_server_session.py:_lookup_pending_file_change (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server_session.py"
    PY_FN = "_lookup_pending_file_change"
    C_FN = "_lookup_pending_file_change"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
