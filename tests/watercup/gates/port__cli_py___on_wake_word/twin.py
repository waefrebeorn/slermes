"""twin for cli.py:_on_wake_word (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_on_wake_word"
    C_FN = "hcli_on_wake_word"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
