"""twin for src/tools/vision.c:vision_supports_media_in_tool_results (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "src/tools/vision.c"
    PY_FN = "vision_supports_media_in_tool_results"
    C_FN = "vision_supports_media_in_tool_results"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
