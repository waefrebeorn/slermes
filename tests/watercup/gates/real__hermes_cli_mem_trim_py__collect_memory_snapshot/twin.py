"""twin for hermes_cli/mem_trim.py:collect_memory_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mem_trim.py"
    PY_FN = "collect_memory_snapshot"
    C_FN = "collect_memory_snapshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
