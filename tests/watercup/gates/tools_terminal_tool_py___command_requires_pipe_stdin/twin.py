"""twin for tools/terminal_tool.py:_command_requires_pipe_stdin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_command_requires_pipe_stdin"
    C_FN = "_command_requires_pipe_stdin"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
