"""twin for tools/terminal_tool.py:_prompt_for_sudo_password (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_prompt_for_sudo_password"
    C_FN = "_prompt_for_sudo_password"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
