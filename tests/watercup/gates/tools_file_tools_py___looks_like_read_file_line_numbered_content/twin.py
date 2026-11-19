"""twin for tools/file_tools.py:_looks_like_read_file_line_numbered_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "_looks_like_read_file_line_numbered_content"
    C_FN = "file_tools_looks_like_read_file_line_numbered_content"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
