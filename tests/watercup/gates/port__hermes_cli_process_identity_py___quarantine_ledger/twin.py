"""twin for hermes_cli/process_identity.py:_quarantine_ledger (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/process_identity.py"
    PY_FN = "_quarantine_ledger"
    C_FN = "process_identity_quarantine_ledger"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
