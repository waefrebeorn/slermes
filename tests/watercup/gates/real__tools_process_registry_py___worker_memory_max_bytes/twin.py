"""twin for tools/process_registry.py:_worker_memory_max_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_worker_memory_max_bytes"
    C_FN = "process_registry_worker_memory_max_bytes"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
