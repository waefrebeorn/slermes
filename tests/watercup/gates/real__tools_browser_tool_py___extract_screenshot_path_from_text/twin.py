"""twin for tools/browser_tool.py:_extract_screenshot_path_from_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_extract_screenshot_path_from_text"
    C_FN = "_extract_screenshot_path_from_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
