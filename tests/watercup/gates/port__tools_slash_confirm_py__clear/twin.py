"""twin for tools/slash_confirm.py:clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/slash_confirm.py"
    PY_FN = "clear"
    C_FN = "tools_slash_confirm_clear"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
