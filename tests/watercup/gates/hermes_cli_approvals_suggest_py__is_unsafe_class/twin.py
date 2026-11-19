"""twin for hermes_cli/approvals_suggest.py:is_unsafe_class (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approvals_suggest.py"
    PY_FN = "is_unsafe_class"
    C_FN = "approval_is_unsafe_class"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
