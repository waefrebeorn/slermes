"""twin for agent/credential_pool.py:_sync_anthropic_entry_from_credentials_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_sync_anthropic_entry_from_credentials_file"
    C_FN = "credential_pool_sync_anthropic_entry_from_credentials_file"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
