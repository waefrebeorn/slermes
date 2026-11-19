"""twin for tools/approval.py:_strip_optional_shell_quotes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_strip_optional_shell_quotes"
    C_FN = "_strip_optional_shell_quotes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
