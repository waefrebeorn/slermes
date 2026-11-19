"""twin for tools/shell_heredoc.py:strip_inert_heredoc_bodies (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/shell_heredoc.py"
    PY_FN = "strip_inert_heredoc_bodies"
    C_FN = "strip_inert_heredoc_bodies"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
