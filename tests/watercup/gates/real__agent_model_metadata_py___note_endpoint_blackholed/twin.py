"""twin for agent/model_metadata.py:_note_endpoint_blackholed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_note_endpoint_blackholed"
    C_FN = "mm_note_endpoint_blackholed"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
