"""twin for hermes_cli/_subprocess_compat.py:harden_git_argv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_subprocess_compat.py"
    PY_FN = "harden_git_argv"
    C_FN = "harden_git_argv"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
