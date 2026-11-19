"""twin for tools/todo_tool.py:check_todo_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/todo_tool.py"
    PY_FN = "check_todo_requirements"
    C_FN = "todo_tool_check_requirements"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
