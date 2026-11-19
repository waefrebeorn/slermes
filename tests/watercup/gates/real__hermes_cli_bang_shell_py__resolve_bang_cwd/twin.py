"""twin for hermes_cli/bang_shell.py:resolve_bang_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bang_shell.py"
    PY_FN = "resolve_bang_cwd"
    C_FN = "resolve_bang_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
