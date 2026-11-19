"""twin for cli.py:_apply_bracketed_paste_timeout_patch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_apply_bracketed_paste_timeout_patch"
    C_FN = "_apply_bracketed_paste_timeout_patch"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
