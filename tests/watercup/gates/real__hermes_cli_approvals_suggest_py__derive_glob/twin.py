"""twin for hermes_cli/approvals_suggest.py:derive_glob (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approvals_suggest.py"
    PY_FN = "derive_glob"
    C_FN = "derive_glob"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
