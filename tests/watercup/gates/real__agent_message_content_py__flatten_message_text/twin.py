"""twin for agent/message_content.py:flatten_message_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_content.py"
    PY_FN = "flatten_message_text"
    C_FN = "flatten_message_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
