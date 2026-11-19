"""twin for agent/credential_pool.py:_prune_stale_seeded_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_prune_stale_seeded_entries"
    C_FN = "cpl_prune_stale_seeded_entries"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
