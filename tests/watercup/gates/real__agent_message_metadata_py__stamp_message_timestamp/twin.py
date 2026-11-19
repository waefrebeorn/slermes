"""twin for agent/message_metadata.py:stamp_message_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_metadata.py"
    PY_FN = "stamp_message_timestamp"
    C_FN = "stamp_message_timestamp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
