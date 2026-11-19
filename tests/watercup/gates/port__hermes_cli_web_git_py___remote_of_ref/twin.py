"""twin for hermes_cli/web_git.py:_remote_of_ref (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_git.py"
    PY_FN = "_remote_of_ref"
    C_FN = "_remote_of_ref"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
