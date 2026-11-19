"""twin for hermes_cli/prompt_stash.py:_clamp_cursor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/prompt_stash.py"
    PY_FN = "_clamp_cursor"
    C_FN = "_clamp_cursor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
