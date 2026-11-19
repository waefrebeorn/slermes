"""twin for cli.py:_terminal_width_for_streaming (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_terminal_width_for_streaming"
    C_FN = "_terminal_width_for_streaming"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
