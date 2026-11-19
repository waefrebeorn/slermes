"""twin for tools/feishu_doc_tool.py:_handle_feishu_doc_read (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/feishu_doc_tool.py"
    PY_FN = "_handle_feishu_doc_read"
    C_FN = "_handle_feishu_doc_read"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
