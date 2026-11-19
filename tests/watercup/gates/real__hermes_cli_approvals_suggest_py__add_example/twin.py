"""twin for hermes_cli/approvals_suggest.py:add_example (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approvals_suggest.py"
    PY_FN = "add_example"
    C_FN = "add_example"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
