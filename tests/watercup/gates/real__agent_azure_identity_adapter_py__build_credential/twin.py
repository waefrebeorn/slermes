"""twin for agent/azure_identity_adapter.py:build_credential (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/azure_identity_adapter.py"
    PY_FN = "build_credential"
    C_FN = "build_credential"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
