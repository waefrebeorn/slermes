"""twin for agent/memory_provider.py:backup_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_provider.py"
    PY_FN = "backup_paths"
    C_FN = "backup_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
