"""twin for gateway/run.py:_adapter_credential_fingerprint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_adapter_credential_fingerprint"
    C_FN = "_adapter_credential_fingerprint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
