"""twin for agent/agent_runtime_helpers.py:sync_credential_pool_entry_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "sync_credential_pool_entry_id"
    C_FN = "sync_credential_pool_entry_id"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
