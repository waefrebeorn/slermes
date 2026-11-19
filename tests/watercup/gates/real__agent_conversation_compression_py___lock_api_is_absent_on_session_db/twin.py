"""twin for agent/conversation_compression.py:_lock_api_is_absent_on_session_db (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_lock_api_is_absent_on_session_db"
    C_FN = "SessionDB"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
