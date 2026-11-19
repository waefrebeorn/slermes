"""twin for tools/terminal_tool.py:_looks_like_help_or_version_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_looks_like_help_or_version_command"
    C_FN = "_looks_like_help_or_version_command"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
