"""twin for agent/secret_sources/onepassword.py:_auth_fingerprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "_auth_fingerprint"
    C_FN = "_auth_fingerprint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
