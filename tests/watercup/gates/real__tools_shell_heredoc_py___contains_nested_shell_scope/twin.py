"""twin for tools/shell_heredoc.py:_contains_nested_shell_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/shell_heredoc.py"
    PY_FN = "_contains_nested_shell_scope"
    C_FN = "_contains_nested_shell_scope"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
