"""twin for tools/tool_output_limits.py:get_max_line_length (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_output_limits.py"
    PY_FN = "get_max_line_length"
    C_FN = "cli_tools_tool_output_limits_get_max_line_length"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
