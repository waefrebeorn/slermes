"""twin for tools/skills_sync_client.py:_opted_in_rel_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "_opted_in_rel_paths"
    C_FN = "_opted_in_rel_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
