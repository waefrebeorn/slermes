"""twin for gateway/message_timestamps.py:render_user_content_with_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/message_timestamps.py"
    PY_FN = "render_user_content_with_timestamp"
    C_FN = "render_user_content_with_timestamp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
