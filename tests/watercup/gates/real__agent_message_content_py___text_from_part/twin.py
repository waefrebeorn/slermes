"""twin for agent/message_content.py:_text_from_part (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_content.py"
    PY_FN = "_text_from_part"
    C_FN = "_text_from_part"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
