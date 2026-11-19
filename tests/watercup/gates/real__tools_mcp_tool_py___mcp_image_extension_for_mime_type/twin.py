"""twin for tools/mcp_tool.py:_mcp_image_extension_for_mime_type (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_mcp_image_extension_for_mime_type"
    C_FN = "_mcp_image_extension_for_mime_type"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
