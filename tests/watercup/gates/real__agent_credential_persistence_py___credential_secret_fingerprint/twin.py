"""twin for agent/credential_persistence.py:_credential_secret_fingerprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"
    PY_FN = "_credential_secret_fingerprint"
    C_FN = "_credential_secret_fingerprint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
