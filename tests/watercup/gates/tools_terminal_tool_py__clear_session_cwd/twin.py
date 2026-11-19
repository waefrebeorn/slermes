"""twin for tools/terminal_tool.py:clear_session_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "clear_session_cwd"
    C_FN = "term_rec_clear_session_cwd"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
