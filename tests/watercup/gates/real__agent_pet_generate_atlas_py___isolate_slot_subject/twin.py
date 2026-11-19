"""twin for agent/pet/generate/atlas.py:_isolate_slot_subject (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/atlas.py"
    PY_FN = "_isolate_slot_subject"
    C_FN = "_isolate_slot_subject"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
