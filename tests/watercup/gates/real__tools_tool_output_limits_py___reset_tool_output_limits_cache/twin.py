"""twin for tools/tool_output_limits.py:_reset_tool_output_limits_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_output_limits.py"
    PY_FN = "_reset_tool_output_limits_cache"
    C_FN = "cli_tools_tool_output_limits__reset_tool_output_limits_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
