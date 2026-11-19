"""twin for tools/terminal_hints.py:_hint_already_exists (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_hints.py"
    PY_FN = "_hint_already_exists"
    C_FN = "_hint_already_exists"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
