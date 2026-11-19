"""twin for tools/computer_use/tool.py:_image_dimensions_from_b64 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "_image_dimensions_from_b64"
    C_FN = "tools_computer_use_tool_u_image_dimensions_from_b64"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
