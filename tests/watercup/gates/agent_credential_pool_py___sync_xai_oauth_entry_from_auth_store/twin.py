"""twin for agent/credential_pool.py:_sync_xai_oauth_entry_from_auth_store (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_sync_xai_oauth_entry_from_auth_store"
    C_FN = "cpl_sync_xai_oauth_entry_from_auth_store"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
