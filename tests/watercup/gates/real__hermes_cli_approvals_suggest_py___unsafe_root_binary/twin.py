"""twin for hermes_cli/approvals_suggest.py:_unsafe_root_binary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approvals_suggest.py"
    PY_FN = "_unsafe_root_binary"
    C_FN = "is_unsafe_root_binary"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
