"""twin for hermes_cli/web_git.py:_entry_staged (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_git.py"
    PY_FN = "_entry_staged"
    C_FN = "entry_staged"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
