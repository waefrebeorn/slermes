"""twin for tools/tts_tool.py:_strip_markdown_for_tts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_strip_markdown_for_tts"
    C_FN = "tts_tool_strip_markdown_for_tts"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
