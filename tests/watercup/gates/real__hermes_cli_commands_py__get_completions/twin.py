"""twin for hermes_cli/commands.py:get_completions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "get_completions"
    C_FN = "get_completions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
