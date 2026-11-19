"""twin for tools/clarify_tool.py:check_clarify_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_tool.py"
    PY_FN = "check_clarify_requirements"
    C_FN = "check_clarify_requirements"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
