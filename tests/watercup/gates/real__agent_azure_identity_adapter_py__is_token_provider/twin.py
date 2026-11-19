"""twin for agent/azure_identity_adapter.py:is_token_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/azure_identity_adapter.py"
    PY_FN = "is_token_provider"
    C_FN = "azu_is_token_provider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
