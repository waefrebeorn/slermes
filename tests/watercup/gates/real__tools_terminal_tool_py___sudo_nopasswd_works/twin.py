"""twin for tools/terminal_tool.py:_sudo_nopasswd_works (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_sudo_nopasswd_works"
    C_FN = "_sudo_nopasswd_works"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
