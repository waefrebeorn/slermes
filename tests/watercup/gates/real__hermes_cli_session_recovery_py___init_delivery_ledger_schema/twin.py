"""twin for hermes_cli/session_recovery.py:_init_delivery_ledger_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_init_delivery_ledger_schema"
    C_FN = "_init_delivery_ledger_schema"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
