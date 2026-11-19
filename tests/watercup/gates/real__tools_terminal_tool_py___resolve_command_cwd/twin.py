"""twin for tools/terminal_tool.py:_resolve_command_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_resolve_command_cwd"
    C_FN = "_resolve_command_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
