"""twin for tools/managed_tool_gateway.py:build_managed_media_uploader (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/managed_tool_gateway.py"
    PY_FN = "build_managed_media_uploader"
    C_FN = "build_managed_media_uploader"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
