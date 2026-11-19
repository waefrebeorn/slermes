"""twin for tools/skills_sync_client.py:is_sync_eligible (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "is_sync_eligible"
    C_FN = "is_sync_eligible"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
