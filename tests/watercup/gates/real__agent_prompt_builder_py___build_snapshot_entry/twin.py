"""twin for agent/prompt_builder.py:_build_snapshot_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_builder.py"
    PY_FN = "_build_snapshot_entry"
    C_FN = "_build_snapshot_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
