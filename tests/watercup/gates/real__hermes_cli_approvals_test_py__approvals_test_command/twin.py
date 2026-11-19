"""twin for hermes_cli/approvals_test.py:approvals_test_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approvals_test.py"
    PY_FN = "approvals_test_command"
    C_FN = "approvals_test_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
