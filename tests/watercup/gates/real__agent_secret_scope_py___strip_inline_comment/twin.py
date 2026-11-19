"""twin for agent/secret_scope.py:_strip_inline_comment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_scope.py"
    PY_FN = "_strip_inline_comment"
    C_FN = "_strip_inline_comment"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
