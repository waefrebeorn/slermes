"""twin for tools/send_message_tool.py:_describe_media_for_mirror (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "_describe_media_for_mirror"
    C_FN = "_describe_media_for_mirror"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
