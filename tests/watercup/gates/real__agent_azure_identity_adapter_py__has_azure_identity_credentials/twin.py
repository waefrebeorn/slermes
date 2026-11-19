"""twin for agent/azure_identity_adapter.py:has_azure_identity_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/azure_identity_adapter.py"
    PY_FN = "has_azure_identity_credentials"
    C_FN = "azu_has_azure_identity_credentials"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
