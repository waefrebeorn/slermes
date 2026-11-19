"""twin for agent/native_compaction.py:prune_pre_checkpoint_items (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/native_compaction.py"
    PY_FN = "prune_pre_checkpoint_items"
    C_FN = "prune_pre_checkpoint_items"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
