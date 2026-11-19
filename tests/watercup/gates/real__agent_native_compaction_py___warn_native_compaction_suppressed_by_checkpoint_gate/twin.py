"""twin for agent/native_compaction.py:_warn_native_compaction_suppressed_by_checkpoint_gate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/native_compaction.py"
    PY_FN = "_warn_native_compaction_suppressed_by_checkpoint_gate"
    C_FN = "_warn_native_compaction_suppressed_by_checkpoint_gate"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
