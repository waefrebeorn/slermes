"""twin for cli.py:_format_submitted_user_message_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_format_submitted_user_message_preview"
    C_FN = "_format_submitted_user_message_preview"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
