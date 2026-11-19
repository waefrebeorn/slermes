"""twin for agent/conversation_compression.py:_capture_authoritative_cooldown_under_lease (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_capture_authoritative_cooldown_under_lease"
    C_FN = "_capture_authoritative_cooldown_under_lease"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
