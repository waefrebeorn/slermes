"""twin for cli.py:_strip_leaked_terminal_responses (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_strip_leaked_terminal_responses"
    C_FN = "_strip_leaked_terminal_responses"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
