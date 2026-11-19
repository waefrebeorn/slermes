"""twin for agent/credential_pool.py:_sync_anthropic_entry_from_pool_store (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_sync_anthropic_entry_from_pool_store"
    C_FN = "_sync_anthropic_entry_from_pool_store"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
