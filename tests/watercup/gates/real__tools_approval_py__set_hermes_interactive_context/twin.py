"""twin for tools/approval.py:set_hermes_interactive_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "set_hermes_interactive_context"
    C_FN = "set_hermes_interactive_context"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
