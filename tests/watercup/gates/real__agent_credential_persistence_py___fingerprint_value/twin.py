"""twin for agent/credential_persistence.py:_fingerprint_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"
    PY_FN = "_fingerprint_value"
    C_FN = "_fingerprint_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
