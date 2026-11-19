"""twin for tools/approval.py:_is_interactive_cli (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_is_interactive_cli"
    C_FN = "_is_interactive_cli"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
