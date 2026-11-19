"""twin for tools/react_to_message_tool.py:check_react_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/react_to_message_tool.py"
    PY_FN = "check_react_requirements"
    C_FN = "react_check_requirements"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
