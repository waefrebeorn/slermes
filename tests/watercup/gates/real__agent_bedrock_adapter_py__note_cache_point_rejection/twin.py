"""twin for agent/bedrock_adapter.py:note_cache_point_rejection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/bedrock_adapter.py"
    PY_FN = "note_cache_point_rejection"
    C_FN = "note_cache_point_rejection"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
