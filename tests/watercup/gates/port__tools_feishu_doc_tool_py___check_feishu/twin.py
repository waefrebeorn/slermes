"""twin for tools/feishu_doc_tool.py:_check_feishu (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/feishu_doc_tool.py"
    PY_FN = "_check_feishu"
    C_FN = "fdt2_check_feishu"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
