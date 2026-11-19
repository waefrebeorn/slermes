"""twin for tools/feishu_drive_tool.py:set_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/feishu_drive_tool.py"
    PY_FN = "set_client"
    C_FN = "cli_tools_feishu_drive_tool_set_client"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
