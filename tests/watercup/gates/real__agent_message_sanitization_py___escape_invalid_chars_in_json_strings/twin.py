"""twin for agent/message_sanitization.py:_escape_invalid_chars_in_json_strings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "_escape_invalid_chars_in_json_strings"
    C_FN = "_escape_invalid_chars_in_json_strings"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
