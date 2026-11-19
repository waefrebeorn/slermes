"""twin for tools/approval.py:_has_allowlist_shell_operator (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "_has_allowlist_shell_operator"
    C_FN = "apr_has_allowlist_shell_operator"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
