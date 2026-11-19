"""twin for hermes_cli/prompt_stash.py:items (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/prompt_stash.py"
    PY_FN = "items"
    C_FN = "items"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
