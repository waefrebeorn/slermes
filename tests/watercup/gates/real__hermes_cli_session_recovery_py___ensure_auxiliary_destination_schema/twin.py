"""twin for hermes_cli/session_recovery.py:_ensure_auxiliary_destination_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_ensure_auxiliary_destination_schema"
    C_FN = "_ensure_auxiliary_destination_schema"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
