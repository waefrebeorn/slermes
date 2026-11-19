"""twin for cli.py:_normalize_slash_confirm_choice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_normalize_slash_confirm_choice"
    C_FN = "_normalize_slash_confirm_choice"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
