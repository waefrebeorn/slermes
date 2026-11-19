"""twin for tools/feishu_drive_tool.py:_handle_reply_comment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/feishu_drive_tool.py"
    PY_FN = "_handle_reply_comment"
    C_FN = "_handle_reply_comment"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
