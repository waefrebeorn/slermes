"""twin for hermes_cli/update_cmd.py:_ensure_non_trampoline_git (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_ensure_non_trampoline_git"
    C_FN = "_ensure_non_trampoline_git"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
