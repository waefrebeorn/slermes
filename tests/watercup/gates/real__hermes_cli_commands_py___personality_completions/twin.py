"""twin for hermes_cli/commands.py:_personality_completions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "_personality_completions"
    C_FN = "_personality_completions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
