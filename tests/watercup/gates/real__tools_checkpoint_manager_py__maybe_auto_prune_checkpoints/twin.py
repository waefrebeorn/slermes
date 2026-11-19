"""twin for tools/checkpoint_manager.py:maybe_auto_prune_checkpoints (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/checkpoint_manager.py"
    PY_FN = "maybe_auto_prune_checkpoints"
    C_FN = "maybe_auto_prune_checkpoints"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
