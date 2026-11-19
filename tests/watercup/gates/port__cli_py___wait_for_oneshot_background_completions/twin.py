"""twin for cli.py:_wait_for_oneshot_background_completions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_wait_for_oneshot_background_completions"
    C_FN = "cli_wait_for_oneshot_background_completions"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
