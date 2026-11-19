"""twin for agent/credential_persistence.py:is_borrowed_credential_source (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"
    PY_FN = "is_borrowed_credential_source"
    C_FN = "is_borrowed_credential_source"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
